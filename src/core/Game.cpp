#include "core/Game.hpp"

#include <algorithm>
#include <climits>
#include <utility>

#include "core/MazeGenerator.hpp"

namespace cr {

Game::Game(GameOptions options)
    : options_(std::move(options)),
      rng_(options_.seed),
      lives_(options_.startLives),
      nextExtraLife_(options_.extraLifeEvery) {
  loadLevel(std::max(1, options_.startLevel));
}

// ---------------------------------------------------------------- level flow

Maze Game::mazeForLevel(int level) const {
  const auto index = static_cast<std::size_t>(level - 1);
  if (index < options_.levels.size()) return options_.levels[index];
  return MazeGenerator::generate(options_.seed * 0x9E3779B97F4A7C15ull + static_cast<std::uint64_t>(level));
}

void Game::loadLevel(int level) {
  level_ = level;
  maze_ = mazeForLevel(level);
  config_ = makeLevelConfig(level);
  packetsEaten_ = 0;
  keyActive_ = false;
  keyTicks_ = 0;
  keySpawnsDone_ = 0;
  const int total = maze_.totalPickups();
  keyThresholds_ = {std::max(1, total * 3 / 10), std::max(2, total * 7 / 10)};

  // Each virus patrols the open tile nearest to "its" corner.
  const std::array<Vec2i, 4> rawCorners{Vec2i{maze_.width() - 2, 1}, Vec2i{1, 1},
                                        Vec2i{maze_.width() - 2, maze_.height() - 2},
                                        Vec2i{1, maze_.height() - 2}};
  for (std::size_t i = 0; i < 4; ++i) {
    int best = INT_MAX;
    for (int y = 0; y < maze_.height(); ++y) {
      for (int x = 0; x < maze_.width(); ++x) {
        const Vec2i p{x, y};
        if (!maze_.isOpen(p)) continue;
        const int d = std::abs(p.x - rawCorners[i].x) + std::abs(p.y - rawCorners[i].y);
        if (d < best) {
          best = d;
          corners_[i] = p;
        }
      }
    }
  }

  resetActors();
  phase_ = Phase::Ready;
  phaseTicks_ = kReadyTicks;
  emit(EventType::LevelStarted, {}, level);
}

void Game::resetActors() {
  const MazeSpawns& s = maze_.spawns();
  player_ = Mover{};
  player_.placeAt(s.player);
  desired_ = Dir::None;
  facing_ = Dir::Left;
  overclockTicks_ = 0;

  for (std::size_t i = 0; i < viruses_.size(); ++i) {
    Virus& v = viruses_[i];
    v = Virus{};
    v.kind = kAllVirusKinds[i];
    v.patrolCorner = corners_[i];
    if (v.kind == VirusKind::Worm) {
      v.state = VirusState::Active;
      v.homeSlot = s.baseSlots[1];
      v.mover.placeAt(s.baseExit);
      v.mover.dir = Dir::Left;
    } else {
      v.state = VirusState::InBase;
      v.homeSlot = s.baseSlots[i - 1];
      v.mover.placeAt(v.homeSlot);
      v.releaseCountdown = config_.releaseTicks[i];
    }
    v.target = v.patrolCorner;
    v.blinkCountdown = rng_.range(config_.blinkMinTicks, config_.blinkMaxTicks);
    v.retargetCountdown = secondsToTicks(2.5);
    v.roamTarget = v.patrolCorner;
  }

  modeIndex_ = 0;
  modeTicks_ = config_.schedule[0].ticks;
  frozenTicks_ = 0;
  combo_ = 0;
}

// ---------------------------------------------------------------- main tick

void Game::tick(const InputFrame& input) {
  ++tick_;
  if (input.dir != Dir::None) desired_ = input.dir;

  switch (phase_) {
    case Phase::Ready:
      if (--phaseTicks_ <= 0) phase_ = Phase::Playing;
      break;
    case Phase::Playing:
      stepPlaying(input);
      break;
    case Phase::Dying:
      if (--phaseTicks_ <= 0) {
        --lives_;
        emit(EventType::LifeLost, {}, lives_);
        if (lives_ <= 0) {
          phase_ = Phase::GameOver;
          emit(EventType::GameOver, {}, score_);
        } else {
          resetActors();
          phase_ = Phase::Ready;
          phaseTicks_ = kReadyTicks;
        }
      }
      break;
    case Phase::LevelClear:
      if (--phaseTicks_ <= 0) loadLevel(level_ + 1);
      break;
    case Phase::GameOver:
      break;
  }
}

void Game::stepPlaying(const InputFrame& input) {
  if (input.overclock && overclockMeter_ >= kOverclockCost && overclockTicks_ == 0) {
    overclockMeter_ = 0;
    overclockTicks_ = kOverclockTicks;
    emit(EventType::OverclockStarted, playerTile());
  }
  if (overclockTicks_ > 0) --overclockTicks_;

  updateModeTimers();
  updatePlayer();
  if (phase_ != Phase::Playing) return;  // level may have been cleared

  for (Virus& v : viruses_) updateVirus(v);
  checkCollisions();

  if (keyActive_ && --keyTicks_ <= 0) {
    keyActive_ = false;
    emit(EventType::KeyExpired, keyTile());
  }
}

void Game::updateModeTimers() {
  if (frozenTicks_ > 0) {
    --frozenTicks_;
    if (frozenTicks_ == config_.freezeWarningTicks) emit(EventType::FreezeEnding);
    if (frozenTicks_ == 0) {
      for (Virus& v : viruses_)
        if (v.state == VirusState::Frozen) v.state = VirusState::Active;
      combo_ = 0;
      emit(EventType::FreezeEnded);
    }
    return;  // the patrol/hunt schedule is paused while the firewall is up
  }
  if (modeTicks_ >= 0 && --modeTicks_ <= 0 && modeIndex_ + 1 < config_.schedule.size()) {
    ++modeIndex_;
    modeTicks_ = config_.schedule[modeIndex_].ticks;
    for (Virus& v : viruses_)
      if (v.state == VirusState::Active) v.reversePending = true;
    emit(EventType::ModeChanged, {}, static_cast<int>(mode()));
  }
}

// ---------------------------------------------------------------- player

void Game::updatePlayer() {
  const int pct = config_.playerSpeedPct + (overclockTicks_ > 0 ? config_.overclockBonusPct : 0);
  // Reversing is always instant, even between tiles.
  if (!player_.atCenter() && desired_ != Dir::None && desired_ == opposite(player_.dir)) player_.dir = desired_;

  auto canEnter = [this](Vec2i t) { return maze_.isOpen(t); };
  advance(
      player_, scaledSpeed(pct), maze_, [this](Vec2i here) { consumeAt(here); },
      [&](Mover& m, Vec2i here) {
        // Buffered input: the last requested direction is taken at the first
        // junction where it becomes possible.
        if (desired_ != Dir::None && canEnter(maze_.wrap(here + delta(desired_)))) m.dir = desired_;
      },
      canEnter);
  if (player_.dir != Dir::None) facing_ = player_.dir;
}

void Game::consumeAt(Vec2i here) {
  const Tile t = maze_.at(here);
  if (t == Tile::Packet || t == Tile::Core) {
    maze_.set(here, Tile::Empty);
    ++packetsEaten_;
    overclockMeter_ = std::min(kOverclockCost, overclockMeter_ + kOverclockPerPickup);
    if (t == Tile::Packet) {
      addScore(kPacketPoints);
      emit(EventType::PacketEaten, here);
    } else {
      addScore(kCorePoints);
      emit(EventType::CoreEaten, here);
      startFreeze();
    }
    maybeSpawnKey();
  }
  if (keyActive_ && here == keyTile()) {
    keyActive_ = false;
    addScore(config_.keyPoints);
    emit(EventType::KeyCollected, here, config_.keyPoints);
  }
  if (maze_.pickupsRemaining() == 0 && phase_ == Phase::Playing) {
    phase_ = Phase::LevelClear;
    phaseTicks_ = kLevelClearTicks;
    emit(EventType::LevelCleared, {}, level_);
  }
}

void Game::addScore(int points) {
  score_ += points;
  while (options_.extraLifeEvery > 0 && score_ >= nextExtraLife_) {
    ++lives_;
    nextExtraLife_ += options_.extraLifeEvery;
    emit(EventType::ExtraLife, {}, lives_);
  }
}

void Game::startFreeze() {
  if (config_.freezeTicks <= 0) return;
  frozenTicks_ = config_.freezeTicks;
  combo_ = 0;
  for (Virus& v : viruses_) {
    if (v.state == VirusState::Active) {
      v.state = VirusState::Frozen;
      v.reversePending = true;
    }
  }
}

void Game::maybeSpawnKey() {
  if (keySpawnsDone_ >= 2 || packetsEaten_ != keyThresholds_[static_cast<std::size_t>(keySpawnsDone_)]) return;
  ++keySpawnsDone_;
  keyActive_ = true;
  keyTicks_ = config_.keyTicks;
  emit(EventType::KeySpawned, keyTile(), config_.keyPoints);
}

// ---------------------------------------------------------------- viruses

void Game::updateVirus(Virus& v) {
  if (v.state == VirusState::InBase) {
    if (v.releaseCountdown > 0) {
      --v.releaseCountdown;
      return;
    }
    v.state = VirusState::Leaving;
  }

  if (v.kind == VirusKind::Glitch && v.state == VirusState::Active) {
    if (--v.retargetCountdown <= 0) {
      v.retargetCountdown = secondsToTicks(2.5);
      v.chasing = !v.chasing;
      if (!v.chasing) v.roamTarget = randomOpenTile();
    }
    if (mode() == Mode::Hunt) {
      --v.blinkCountdown;
      if (v.blinkCountdown == kTelegraphTicks) emit(EventType::GlitchTelegraph, maze_.wrap(v.mover.tile()));
      if (v.blinkCountdown <= 0) blink(v);
    }
  }

  int pct = config_.virusSpeedPct;
  switch (v.state) {
    case VirusState::Leaving: pct = config_.leavingSpeedPct; break;
    case VirusState::Frozen: pct = config_.frozenSpeedPct; break;
    case VirusState::Quarantined: pct = config_.quarantinedSpeedPct; break;
    default: break;
  }

  bool allowReverse = false;
  if (v.reversePending) {
    v.reversePending = false;
    if (!v.mover.atCenter() && v.mover.dir != Dir::None)
      v.mover.dir = opposite(v.mover.dir);
    else
      allowReverse = true;
  }

  auto canEnter = [this, &v](Vec2i t) {
    const bool baseTraffic = v.state == VirusState::Leaving || v.state == VirusState::Quarantined;
    return baseTraffic ? maze_.isOpenForBaseTraffic(t) : maze_.isOpen(t);
  };
  advance(
      v.mover, scaledSpeed(pct), maze_, [&](Vec2i here) { onVirusCenter(v, here); },
      [&](Mover& m, Vec2i here) {
        m.dir = decideVirusDirection(v, here, allowReverse);
        allowReverse = false;
      },
      canEnter);
}

void Game::onVirusCenter(Virus& v, Vec2i here) {
  if (v.state == VirusState::Leaving && here == maze_.spawns().baseExit) {
    v.state = VirusState::Active;
    v.mover.dir = Dir::Left;
  } else if (v.state == VirusState::Quarantined && here == v.homeSlot) {
    v.state = VirusState::InBase;
    v.releaseCountdown = secondsToTicks(1.0);
    v.mover.dir = Dir::None;
  }
}

Dir Game::decideVirusDirection(Virus& v, Vec2i here, bool allowReverse) {
  if (v.state == VirusState::InBase) return Dir::None;

  const bool baseTraffic = v.state == VirusState::Leaving || v.state == VirusState::Quarantined;
  auto passable = [&](Vec2i t) { return baseTraffic ? maze_.isOpenForBaseTraffic(t) : maze_.isOpen(t); };

  if (v.state == VirusState::Frozen) {
    // Panicked: random choice at each junction, avoiding U-turns.
    std::array<Dir, 4> options{};
    int count = 0;
    for (Dir d : kAllDirs) {
      if (!allowReverse && v.mover.dir != Dir::None && d == opposite(v.mover.dir)) continue;
      if (passable(maze_.wrap(here + delta(d)))) options[static_cast<std::size_t>(count++)] = d;
    }
    if (count == 0) return opposite(v.mover.dir);
    return options[static_cast<std::size_t>(rng_.uniform(count))];
  }

  const Vec2i target = computeTarget(v);
  v.target = target;
  field_.compute(maze_, target, passable);
  return chooseDirection(maze_, here, v.mover.dir, field_, target, allowReverse, passable);
}

Vec2i Game::computeTarget(const Virus& v) {
  switch (v.state) {
    case VirusState::Leaving: return maze_.spawns().baseExit;
    case VirusState::Quarantined: return v.homeSlot;
    default: break;
  }
  if (mode() == Mode::Patrol) return v.patrolCorner;

  switch (v.kind) {
    case VirusKind::Worm: return playerTile();
    case VirusKind::Trojan: return predictPlayerTile(5);
    case VirusKind::Rootkit: return pincerTarget(v);
    case VirusKind::Glitch: return v.chasing ? playerTile() : v.roamTarget;
  }
  return playerTile();
}

Vec2i Game::predictPlayerTile(int steps) const {
  Vec2i pos = playerTile();
  Dir d = facing_;
  if (desired_ != Dir::None && maze_.isOpen(maze_.wrap(pos + delta(desired_)))) d = desired_;
  for (int i = 0; i < steps; ++i) {
    Vec2i next = maze_.wrap(pos + delta(d));
    if (!maze_.isOpen(next)) {
      Dir turn = Dir::None;
      for (Dir c : kAllDirs) {
        if (c == d || c == opposite(d)) continue;
        if (maze_.isOpen(maze_.wrap(pos + delta(c)))) {
          turn = c;
          break;
        }
      }
      if (turn == Dir::None) break;
      d = turn;
      next = maze_.wrap(pos + delta(d));
    }
    pos = next;
  }
  return pos;
}

Vec2i Game::pincerTarget(const Virus& self) {
  const Vec2i p = playerTile();
  auto open = [this](Vec2i t) { return maze_.isOpen(t); };
  scratchA_.compute(maze_, p, open);
  if (scratchA_.at(maze_.wrap(self.mover.tile())) <= 3) return p;  // close enough: just go for it

  scratchB_.compute(maze_, maze_.wrap(virus(VirusKind::Worm).mover.tile()), open);
  Vec2i best = p;
  int bestScore = INT_MIN;
  for (int y = 0; y < maze_.height(); ++y) {
    for (int x = 0; x < maze_.width(); ++x) {
      const Vec2i t{x, y};
      const int dp = scratchA_.at(t);
      if (dp < 2 || dp > 6) continue;
      int dw = scratchB_.at(t);
      if (dw == DistanceField::kUnreachable) dw = 64;
      const int score = dw - 2 * dp;
      if (score > bestScore) {
        bestScore = score;
        best = t;
      }
    }
  }
  return best;
}

Vec2i Game::randomOpenTile() {
  for (int attempt = 0; attempt < 200; ++attempt) {
    const Vec2i t{rng_.uniform(maze_.width()), rng_.uniform(maze_.height())};
    if (maze_.isOpen(t)) return t;
  }
  return maze_.spawns().player;
}

void Game::blink(Virus& v) {
  v.blinkCountdown = rng_.range(config_.blinkMinTicks, config_.blinkMaxTicks);
  auto open = [this](Vec2i t) { return maze_.isOpen(t); };
  scratchA_.compute(maze_, playerTile(), open);
  std::vector<Vec2i> candidates;
  for (int y = 0; y < maze_.height(); ++y) {
    for (int x = 0; x < maze_.width(); ++x) {
      const int d = scratchA_.at({x, y});
      if (d >= 9 && d <= 15) candidates.push_back({x, y});
    }
  }
  if (candidates.empty()) return;
  const Vec2i from = maze_.wrap(v.mover.tile());
  const Vec2i to = candidates[static_cast<std::size_t>(rng_.uniform(static_cast<int>(candidates.size())))];
  v.mover.placeAt(to);
  v.mover.dir = Dir::None;
  emit(EventType::GlitchBlink, to, 0, from);
}

void Game::checkCollisions() {
  for (Virus& v : viruses_) {
    if (!v.dangerous() && !v.catchable()) continue;
    if (wrappedDistance(player_, v.mover, maze_) >= kCollisionRadius) continue;
    if (v.catchable()) {
      v.state = VirusState::Quarantined;
      const int points = kVirusBasePoints << std::min(combo_, 3);
      ++combo_;
      addScore(points);
      emit(EventType::VirusQuarantined, maze_.wrap(v.mover.tile()), points);
    } else {
      phase_ = Phase::Dying;
      phaseTicks_ = kDyingTicks;
      emit(EventType::PlayerCaught, playerTile());
      return;
    }
  }
}

// ---------------------------------------------------------------- misc

void Game::emit(EventType type, Vec2i tile, int value, Vec2i tile2) {
  events_.push_back(GameEvent{type, tile, tile2, value});
}

std::vector<GameEvent> Game::drainEvents() {
  std::vector<GameEvent> out;
  out.swap(events_);
  return out;
}

std::uint64_t Game::stateHash() const {
  std::uint64_t h = 0xcbf29ce484222325ull;
  auto mix = [&h](std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      h ^= (v >> (i * 8)) & 0xFF;
      h *= 0x100000001b3ull;
    }
  };
  mix(tick_);
  mix(static_cast<std::uint64_t>(score_));
  mix(static_cast<std::uint64_t>(lives_));
  mix(static_cast<std::uint64_t>(level_));
  mix(static_cast<std::uint64_t>(phase_));
  mix(static_cast<std::uint64_t>(player_.px));
  mix(static_cast<std::uint64_t>(player_.py));
  mix(static_cast<std::uint64_t>(player_.dir));
  for (const Virus& v : viruses_) {
    mix(static_cast<std::uint64_t>(v.state));
    mix(static_cast<std::uint64_t>(v.mover.px));
    mix(static_cast<std::uint64_t>(v.mover.py));
  }
  mix(rng_.state());
  mix(static_cast<std::uint64_t>(maze_.pickupsRemaining()));
  return h;
}

}  // namespace cr
