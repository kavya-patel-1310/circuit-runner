#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/Events.hpp"
#include "core/LevelConfig.hpp"
#include "core/Maze.hpp"
#include "core/Mover.hpp"
#include "core/Pathfinding.hpp"
#include "core/Rng.hpp"
#include "core/Virus.hpp"

namespace cr {

enum class Phase : std::uint8_t { Ready, Playing, Dying, LevelClear, GameOver };

/// Everything the simulation needs from the outside world for one tick.
struct InputFrame {
  Dir dir = Dir::None;
  bool overclock = false;
  friend bool operator==(const InputFrame&, const InputFrame&) = default;
};

struct GameOptions {
  std::uint64_t seed = 0xC0FFEE;
  std::vector<Maze> levels;  // hand-made levels are played first, then procedural ones
  int startLives = 3;
  int extraLifeEvery = 10000;
  int startLevel = 1;
};

/// The complete, headless, deterministic game simulation.
/// Same options + same input sequence => bit-identical game. No graphics,
/// audio or OS dependencies, so it is fully unit-testable.
class Game {
 public:
  static constexpr int kReadyTicks = 120;
  static constexpr int kDyingTicks = 100;
  static constexpr int kLevelClearTicks = 150;
  static constexpr int kPacketPoints = 10;
  static constexpr int kCorePoints = 50;
  static constexpr int kVirusBasePoints = 200;
  static constexpr int kOverclockCost = 100;
  static constexpr int kOverclockPerPickup = 2;
  static constexpr int kOverclockTicks = 180;
  static constexpr int kCollisionRadius = kSub * 6 / 10;
  static constexpr int kTelegraphTicks = 60;

  explicit Game(GameOptions options);

  void tick(const InputFrame& input);
  std::vector<GameEvent> drainEvents();

  // ---- read-only state (renderer, AI, tests) ----
  const GameOptions& options() const { return options_; }
  const Maze& maze() const { return maze_; }
  const LevelConfig& config() const { return config_; }
  Phase phase() const { return phase_; }
  int phaseTicks() const { return phaseTicks_; }
  int score() const { return score_; }
  int lives() const { return lives_; }
  int level() const { return level_; }
  std::uint64_t tickCount() const { return tick_; }
  const Mover& player() const { return player_; }
  Vec2i playerTile() const { return maze_.wrap(player_.tile()); }
  Dir playerFacing() const { return facing_; }
  const std::array<Virus, 4>& viruses() const { return viruses_; }
  const Virus& virus(VirusKind k) const { return viruses_[static_cast<std::size_t>(k)]; }
  Mode mode() const { return config_.schedule[modeIndex_].mode; }
  int frozenTicks() const { return frozenTicks_; }
  bool keyActive() const { return keyActive_; }
  Vec2i keyTile() const { return maze_.spawns().keySpawn; }
  int keyTicks() const { return keyTicks_; }
  int overclockMeter() const { return overclockMeter_; }
  int overclockTicks() const { return overclockTicks_; }
  int packetsEaten() const { return packetsEaten_; }
  int combo() const { return combo_; }

  /// Order-sensitive hash of the full simulation state (used to verify replays).
  std::uint64_t stateHash() const;

  /// Where the player will be after `steps` tiles if they keep going.
  Vec2i predictPlayerTile(int steps) const;
  /// Target tile the given virus is currently steering towards.
  Vec2i computeTarget(const Virus& v);

  // ---- test hooks ----
  Maze& mutableMaze() { return maze_; }
  Mover& mutablePlayer() { return player_; }
  Virus& mutableVirus(VirusKind k) { return viruses_[static_cast<std::size_t>(k)]; }
  void forcePhase(Phase p, int ticks = 0) {
    phase_ = p;
    phaseTicks_ = ticks;
  }

 private:
  Maze mazeForLevel(int level) const;
  void loadLevel(int level);
  void resetActors();
  void stepPlaying(const InputFrame& input);
  void updateModeTimers();
  void updatePlayer();
  void consumeAt(Vec2i tile);
  void addScore(int points);
  void startFreeze();
  void maybeSpawnKey();
  void updateVirus(Virus& v);
  Dir decideVirusDirection(Virus& v, Vec2i here, bool allowReverse);
  void onVirusCenter(Virus& v, Vec2i here);
  Vec2i pincerTarget(const Virus& self);
  Vec2i randomOpenTile();
  void blink(Virus& v);
  void checkCollisions();
  void emit(EventType type, Vec2i tile = {}, int value = 0, Vec2i tile2 = {});

  GameOptions options_;
  Rng rng_;
  Maze maze_;
  LevelConfig config_;
  Phase phase_ = Phase::Ready;
  int phaseTicks_ = 0;
  std::uint64_t tick_ = 0;
  int score_ = 0;
  int lives_ = 3;
  int nextExtraLife_ = 10000;
  int level_ = 1;

  Mover player_;
  Dir desired_ = Dir::None;
  Dir facing_ = Dir::Left;
  std::array<Virus, 4> viruses_{};
  std::array<Vec2i, 4> corners_{};

  std::size_t modeIndex_ = 0;
  int modeTicks_ = 0;
  int frozenTicks_ = 0;
  int combo_ = 0;

  int packetsEaten_ = 0;
  bool keyActive_ = false;
  int keyTicks_ = 0;
  int keySpawnsDone_ = 0;
  std::array<int, 2> keyThresholds_{};

  int overclockMeter_ = 0;
  int overclockTicks_ = 0;

  DistanceField field_;
  DistanceField scratchA_;
  DistanceField scratchB_;
  std::vector<GameEvent> events_;
};

}  // namespace cr
