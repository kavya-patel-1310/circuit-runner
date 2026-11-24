#include "app/Renderer.hpp"

#include <algorithm>
#include <cmath>

namespace cr::app {

namespace {

Vector2 dirVector(Dir d) {
  const Vec2i v = delta(d);
  return {static_cast<float>(v.x), static_cast<float>(v.y)};
}

Color withAlpha(Color c, float a) {
  c.a = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(c.a));
  return c;
}

Color lerpColor(Color a, Color b, float t) {
  auto mix = [t](unsigned char x, unsigned char y) {
    return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
  };
  return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

}  // namespace

Color virusColor(VirusKind kind) {
  switch (kind) {
    case VirusKind::Worm: return {255, 77, 109, 255};     // crimson
    case VirusKind::Trojan: return {179, 136, 255, 255};  // violet
    case VirusKind::Rootkit: return {156, 255, 87, 255};  // lime
    case VirusKind::Glitch: return {255, 179, 71, 255};   // amber
  }
  return WHITE;
}

// ---------------------------------------------------------------- effects

void Renderer::onEvent(const GameEvent& e, const Game& game) {
  layout(game.maze());
  const Vector2 at = tileCenter(e.tile);
  switch (e.type) {
    case EventType::PacketEaten: burst(at, palette::Packet, 3, 40, 0.25f); break;
    case EventType::CoreEaten:
      burst(at, palette::Core, 24, 160, 0.6f);
      flash_ = 0.25f;
      break;
    case EventType::VirusQuarantined:
      burst(at, palette::Frozen, 30, 200, 0.7f);
      texts_.push_back({at, std::to_string(e.value), 1.0f, palette::Accent});
      break;
    case EventType::PlayerCaught:
      burst(at, palette::Drone, 60, 260, 1.2f);
      shake_ = 0.5f;
      break;
    case EventType::KeyCollected:
      burst(at, palette::Warning, 30, 180, 0.8f);
      texts_.push_back({at, std::to_string(e.value), 1.2f, palette::Warning});
      break;
    case EventType::GlitchBlink:
      burst(tileCenter(e.tile2), virusColor(VirusKind::Glitch), 20, 120, 0.4f);
      burst(at, virusColor(VirusKind::Glitch), 20, 120, 0.4f);
      break;
    case EventType::OverclockStarted: burst(at, palette::Accent, 30, 220, 0.5f); break;
    case EventType::ExtraLife:
      texts_.push_back({tileCenter(game.playerTile()), "1UP", 1.5f, palette::Accent});
      break;
    default: break;
  }
}

void Renderer::burst(Vector2 at, Color color, int count, float speed, float life) {
  for (int i = 0; i < count; ++i) {
    const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
    const float s = speed * static_cast<float>(GetRandomValue(30, 100)) / 100.0f;
    particles_.push_back({at, {std::cos(angle) * s, std::sin(angle) * s}, life, life,
                          static_cast<float>(GetRandomValue(15, 35)) / 10.0f, color});
  }
}

void Renderer::update(float dt) {
  time_ += dt;
  shake_ = std::max(0.0f, shake_ - dt);
  flash_ = std::max(0.0f, flash_ - dt);
  for (auto& p : particles_) {
    p.pos.x += p.vel.x * dt;
    p.pos.y += p.vel.y * dt;
    p.vel.x *= 0.92f;
    p.vel.y *= 0.92f;
    p.life -= dt;
  }
  std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.0f; });
  for (auto& t : texts_) {
    t.pos.y -= 30.0f * dt;
    t.life -= dt;
  }
  std::erase_if(texts_, [](const FloatingText& t) { return t.life <= 0.0f; });
}

void Renderer::clearEffects() {
  particles_.clear();
  texts_.clear();
  shake_ = flash_ = 0.0f;
}

// ---------------------------------------------------------------- layout

void Renderer::layout(const Maze& maze) {
  const float areaH = static_cast<float>(kCanvasH - kHudTop - kHudBottom);
  tile_ = std::floor(std::min(static_cast<float>(kCanvasW) / static_cast<float>(maze.width()),
                              areaH / static_cast<float>(maze.height())));
  origin_.x = (static_cast<float>(kCanvasW) - tile_ * static_cast<float>(maze.width())) / 2.0f;
  origin_.y = static_cast<float>(kHudTop) + (areaH - tile_ * static_cast<float>(maze.height())) / 2.0f;
}

Vector2 Renderer::tileCenter(Vec2i t) const {
  return {origin_.x + (static_cast<float>(t.x) + 0.5f) * tile_, origin_.y + (static_cast<float>(t.y) + 0.5f) * tile_};
}

Vector2 Renderer::moverCenter(const Mover& m) const {
  return {origin_.x + (static_cast<float>(m.px) / kSub + 0.5f) * tile_,
          origin_.y + (static_cast<float>(m.py) / kSub + 0.5f) * tile_};
}

// ---------------------------------------------------------------- characters

void Renderer::drawDrone(Vector2 c, float r, Dir facing, float time, float alpha) {
  // Four rotors on the diagonals, spinning.
  for (int i = 0; i < 4; ++i) {
    const float a = (45.0f + 90.0f * static_cast<float>(i)) * DEG2RAD;
    const Vector2 p{c.x + std::cos(a) * r * 0.82f, c.y + std::sin(a) * r * 0.82f};
    DrawLineEx(c, p, 2.0f, withAlpha(palette::Dim, alpha));
    DrawCircleLinesV(p, r * 0.34f, withAlpha(palette::Drone, alpha * 0.8f));
    const float spin = time * 40.0f + static_cast<float>(i);
    const Vector2 b{std::cos(spin) * r * 0.3f, std::sin(spin) * r * 0.3f};
    DrawLineEx({p.x - b.x, p.y - b.y}, {p.x + b.x, p.y + b.y}, 1.5f, withAlpha(WHITE, alpha * 0.9f));
  }
  // Hexagonal chassis.
  DrawPoly(c, 6, r * 0.62f, 30.0f, withAlpha(palette::Drone, alpha));
  DrawPolyLinesEx(c, 6, r * 0.62f, 30.0f, 1.5f, withAlpha(WHITE, alpha));
  // Camera lens looking in the direction of travel.
  const Vector2 f = dirVector(facing == Dir::None ? Dir::Right : facing);
  const Vector2 lens{c.x + f.x * r * 0.22f, c.y + f.y * r * 0.22f};
  DrawCircleV(lens, r * 0.24f, withAlpha({10, 30, 45, 255}, alpha));
  DrawCircleV({lens.x + f.x * r * 0.06f, lens.y + f.y * r * 0.06f}, r * 0.09f, withAlpha(palette::Accent, alpha));
}

void Renderer::drawVirusShape(VirusKind kind, Vector2 c, float r, float time, Color color, Dir facing) {
  int sides = 0, spikes = 8;
  float spin = time * 0.6f;
  switch (kind) {
    case VirusKind::Worm: sides = 0; spikes = 10; break;
    case VirusKind::Trojan: sides = 4; spikes = 8; spin = -time * 0.5f; break;
    case VirusKind::Rootkit: sides = 3; spikes = 9; spin = time * 0.9f; break;
    case VirusKind::Glitch: sides = 5; spikes = 7; spin = time * 1.4f; break;
  }
  // Spikes with bulbs at the tips.
  for (int i = 0; i < spikes; ++i) {
    const float a = spin + static_cast<float>(i) * 2.0f * PI / static_cast<float>(spikes);
    const float wobble = 1.0f + 0.08f * std::sin(time * 6.0f + static_cast<float>(i));
    const Vector2 tip{c.x + std::cos(a) * r * 0.95f * wobble, c.y + std::sin(a) * r * 0.95f * wobble};
    DrawLineEx(c, tip, 2.0f, color);
    DrawCircleV(tip, r * 0.12f, color);
  }
  const Color body = lerpColor(color, BLACK, 0.25f);
  if (sides == 0) {
    DrawCircleV(c, r * 0.58f, body);
    DrawCircleLinesV(c, r * 0.58f, color);
  } else {
    DrawPoly(c, sides, r * 0.66f, spin * RAD2DEG, body);
    DrawPolyLinesEx(c, sides, r * 0.66f, spin * RAD2DEG, 1.5f, color);
  }
  // Scanner slit that slides toward the direction of travel.
  const Vector2 f = dirVector(facing);
  const Vector2 s{c.x + f.x * r * 0.18f, c.y + f.y * r * 0.18f};
  DrawRectangleRounded({s.x - r * 0.34f, s.y - r * 0.08f, r * 0.68f, r * 0.16f}, 1.0f, 4, {255, 255, 255, 230});
}

void Renderer::drawTextCentered(const std::string& text, int cx, int y, int size, Color color) {
  const int w = MeasureText(text.c_str(), size);
  DrawText(text.c_str(), cx - w / 2, y, size, color);
}

// ---------------------------------------------------------------- scene

void Renderer::draw(const Game& game, const DrawOptions& options) {
  layout(game.maze());
  if (shake_ > 0.0f) {
    origin_.x += static_cast<float>(GetRandomValue(-4, 4)) * shake_;
    origin_.y += static_cast<float>(GetRandomValue(-4, 4)) * shake_;
  }

  ClearBackground(palette::Background);
  for (int x = 0; x < kCanvasW; x += 12) DrawLine(x, kHudTop, x, kCanvasH - kHudBottom, palette::GridLine);
  for (int y = kHudTop; y < kCanvasH - kHudBottom; y += 12) DrawLine(0, y, kCanvasW, y, palette::GridLine);

  drawMaze(game);
  drawPickups(game);
  drawViruses(game, options);
  drawPlayer(game);

  for (const auto& p : particles_) {
    const float a = p.life / p.maxLife;
    DrawRectangleV({p.pos.x - p.size / 2, p.pos.y - p.size / 2}, {p.size, p.size}, withAlpha(p.color, a));
  }
  for (const auto& t : texts_) {
    const int w = MeasureText(t.text.c_str(), 16);
    DrawText(t.text.c_str(), static_cast<int>(t.pos.x) - w / 2, static_cast<int>(t.pos.y) - 8, 16,
             withAlpha(t.color, std::min(1.0f, t.life * 2.0f)));
  }
  if (flash_ > 0.0f) DrawRectangle(0, 0, kCanvasW, kCanvasH, withAlpha(palette::Core, flash_ * 0.4f));

  if (options.dimmed) {
    DrawRectangle(0, 0, kCanvasW, kCanvasH, {0, 0, 0, 150});
    return;
  }
  drawHud(game, options);
  drawOverlayText(game);
}

void Renderer::drawMaze(const Game& game) {
  const Maze& m = game.maze();
  // Walls flash white-green when the level is cleared.
  Color edge = palette::WallEdge;
  if (game.phase() == Phase::LevelClear && static_cast<int>(time_ * 6.0f) % 2 == 0) edge = WHITE;

  const float inset = std::max(2.0f, tile_ * 0.12f);
  for (int y = 0; y < m.height(); ++y) {
    for (int x = 0; x < m.width(); ++x) {
      const Vec2i p{x, y};
      const float px = origin_.x + static_cast<float>(x) * tile_;
      const float py = origin_.y + static_cast<float>(y) * tile_;
      const Tile t = m.at(p);
      if (t == Tile::Gate) {
        for (float gx = px; gx < px + tile_; gx += 6.0f)
          DrawRectangleV({gx, py + tile_ * 0.42f}, {3.0f, tile_ * 0.16f}, palette::Gate);
        continue;
      }
      if (t != Tile::Wall) continue;
      DrawRectangleV({px, py}, {tile_, tile_}, palette::WallFill);
      // Bright trace along every edge that faces open floor.
      auto openAt = [&](int dx, int dy) {
        const int nx = x + dx, ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) return false;
        return !m.isWall({nx, ny});
      };
      if (openAt(0, -1)) DrawLineEx({px, py + inset}, {px + tile_, py + inset}, 2.0f, edge);
      if (openAt(0, 1)) DrawLineEx({px, py + tile_ - inset}, {px + tile_, py + tile_ - inset}, 2.0f, edge);
      if (openAt(-1, 0)) DrawLineEx({px + inset, py}, {px + inset, py + tile_}, 2.0f, edge);
      if (openAt(1, 0)) DrawLineEx({px + tile_ - inset, py}, {px + tile_ - inset, py + tile_}, 2.0f, edge);
      // Decorative solder pads.
      if ((x * 7 + y * 13) % 11 == 0) DrawCircleV({px + tile_ / 2, py + tile_ / 2}, 2.0f, {60, 110, 120, 255});
    }
  }
}

void Renderer::drawPickups(const Game& game) {
  const Maze& m = game.maze();
  for (int y = 0; y < m.height(); ++y) {
    for (int x = 0; x < m.width(); ++x) {
      const Tile t = m.at({x, y});
      const Vector2 c = tileCenter({x, y});
      if (t == Tile::Packet) {
        const float s = tile_ * 0.14f;
        DrawPoly(c, 4, s, 45.0f, palette::Packet);
      } else if (t == Tile::Core) {
        const float pulse = 0.75f + 0.25f * std::sin(time_ * 6.0f + static_cast<float>(x));
        DrawPoly(c, 6, tile_ * 0.38f * pulse, time_ * 60.0f, withAlpha(palette::Core, 0.35f));
        DrawPoly(c, 6, tile_ * 0.26f, time_ * 60.0f, palette::Core);
        DrawPolyLinesEx(c, 6, tile_ * 0.26f, time_ * 60.0f, 1.5f, WHITE);
      }
    }
  }
  if (game.keyActive()) {
    const Vector2 c = tileCenter(game.keyTile());
    const float bob = std::sin(time_ * 5.0f) * 2.0f;
    const bool blink = game.keyTicks() < 120 && static_cast<int>(time_ * 10.0f) % 2 == 0;
    if (!blink) {
      DrawCircleLinesV({c.x - tile_ * 0.15f, c.y + bob}, tile_ * 0.2f, palette::Warning);
      DrawLineEx({c.x + tile_ * 0.05f, c.y + bob}, {c.x + tile_ * 0.42f, c.y + bob}, 3.0f, palette::Warning);
      DrawLineEx({c.x + tile_ * 0.32f, c.y + bob}, {c.x + tile_ * 0.32f, c.y + bob + tile_ * 0.15f}, 3.0f,
                 palette::Warning);
    }
  }
}

void Renderer::drawViruses(const Game& game, const DrawOptions& options) {
  const float r = tile_ * 0.62f;
  for (const Virus& v : game.viruses()) {
    const Vector2 c = moverCenter(v.mover);
    const Color base = virusColor(v.kind);

    if (options.showTargets && (v.state == VirusState::Active || v.state == VirusState::Leaving)) {
      const Vector2 t = tileCenter(v.target);
      DrawLineEx(c, t, 1.5f, withAlpha(base, 0.5f));
      DrawRectangleLinesEx({t.x - tile_ / 2, t.y - tile_ / 2, tile_, tile_}, 2.0f, base);
    }

    if (game.phase() == Phase::Dying && game.phaseTicks() < Game::kDyingTicks - 30) continue;

    switch (v.state) {
      case VirusState::Quarantined: {
        // Just a compressed outline streaking home.
        DrawPolyLinesEx(c, 4, r * 0.35f, time_ * 400.0f, 2.0f, withAlpha(base, 0.8f));
        break;
      }
      case VirusState::Frozen: {
        const bool ending = game.frozenTicks() < game.config().freezeWarningTicks;
        const bool white = ending && static_cast<int>(time_ * 8.0f) % 2 == 0;
        const Color col = white ? WHITE : palette::Frozen;
        drawVirusShape(v.kind, c, r * 0.9f, time_ * 0.2f, col, v.mover.dir);
        // Padlock glyph: they're locked down by the firewall.
        DrawRectangleV({c.x - r * 0.18f, c.y - r * 0.02f}, {r * 0.36f, r * 0.28f}, {30, 40, 50, 255});
        DrawCircleLinesV({c.x, c.y - r * 0.05f}, r * 0.14f, {30, 40, 50, 255});
        break;
      }
      default: {
        float alpha = 1.0f;
        Vector2 pos = c;
        if (v.kind == VirusKind::Glitch && v.state == VirusState::Active && game.mode() == Mode::Hunt &&
            v.blinkCountdown <= Game::kTelegraphTicks) {
          // Telegraph an imminent teleport with RGB-split jitter.
          alpha = 0.5f + 0.5f * std::sin(time_ * 60.0f);
          drawVirusShape(v.kind, {pos.x - 3, pos.y}, r, time_, withAlpha({255, 0, 80, 255}, 0.5f), v.mover.dir);
          drawVirusShape(v.kind, {pos.x + 3, pos.y}, r, time_, withAlpha({0, 200, 255, 255}, 0.5f), v.mover.dir);
        }
        if (v.state == VirusState::InBase) pos.y += std::sin(time_ * 4.0f + static_cast<float>(v.kind)) * 3.0f;
        drawVirusShape(v.kind, pos, r, time_, withAlpha(base, alpha), v.mover.dir == Dir::None ? Dir::Down : v.mover.dir);
        break;
      }
    }
  }
}

void Renderer::drawPlayer(const Game& game) {
  if (game.phase() == Phase::GameOver) return;
  const Vector2 c = moverCenter(game.player());
  const float r = tile_ * 0.72f;
  if (game.phase() == Phase::Dying) {
    const float t = 1.0f - static_cast<float>(game.phaseTicks()) / Game::kDyingTicks;
    if (t < 0.35f) drawDrone(c, r * (1.0f + t), game.playerFacing(), time_ * (1.0f + t * 10.0f), 1.0f - t * 2.0f);
    return;
  }
  if (game.overclockTicks() > 0) {
    // Speed trail.
    const Vector2 f = dirVector(game.playerFacing());
    for (int i = 1; i <= 4; ++i)
      DrawCircleV({c.x - f.x * tile_ * 0.35f * static_cast<float>(i), c.y - f.y * tile_ * 0.35f * static_cast<float>(i)},
                  r * 0.35f / static_cast<float>(i), withAlpha(palette::Accent, 0.5f));
  }
  drawDrone(c, r, game.playerFacing(), time_);
}

void Renderer::drawHud(const Game& game, const DrawOptions& options) {
  DrawRectangle(0, 0, kCanvasW, kHudTop, {6, 10, 20, 255});
  DrawLine(0, kHudTop - 1, kCanvasW, kHudTop - 1, palette::WallEdge);

  DrawText("SCORE", 16, 8, 14, palette::Dim);
  DrawText(TextFormat("%07d", game.score()), 16, 24, 24, palette::Text);
  drawTextCentered("HIGH", kCanvasW / 2, 8, 14, palette::Dim);
  drawTextCentered(TextFormat("%07d", std::max(options.highScore, game.score())), kCanvasW / 2, 24, 24, palette::Text);

  const std::string lvl = TextFormat("LEVEL %d", game.level());
  DrawText(lvl.c_str(), kCanvasW - 16 - MeasureText(lvl.c_str(), 14), 8, 14, palette::Dim);
  const std::string& name = game.maze().name();
  DrawText(name.c_str(), kCanvasW - 16 - MeasureText(name.c_str(), 20), 26, 20, palette::Accent);

  // Bottom bar: spare lives, overclock meter, current threat mode.
  const int by = kCanvasH - kHudBottom;
  DrawRectangle(0, by, kCanvasW, kHudBottom, {6, 10, 20, 255});
  DrawLine(0, by, kCanvasW, by, palette::WallEdge);
  for (int i = 0; i < std::min(game.lives() - 1, 6); ++i)
    drawDrone({28.0f + static_cast<float>(i) * 30.0f, static_cast<float>(by) + 24.0f}, 11.0f, Dir::Right, time_);

  const int mx = 230, mw = 180, my = by + 16;
  DrawText("OVERCLOCK", mx, by + 4, 10, palette::Dim);
  DrawRectangleLines(mx, my, mw, 14, palette::Dim);
  const float fill = game.overclockTicks() > 0 ? static_cast<float>(game.overclockTicks()) / Game::kOverclockTicks
                                               : static_cast<float>(game.overclockMeter()) / Game::kOverclockCost;
  const bool ready = game.overclockMeter() >= Game::kOverclockCost;
  Color meter = ready && static_cast<int>(time_ * 4.0f) % 2 == 0 ? WHITE : palette::Accent;
  DrawRectangle(mx + 2, my + 2, static_cast<int>(static_cast<float>(mw - 4) * fill), 10, meter);
  if (ready) DrawText("SPACE", mx + mw + 8, my, 14, palette::Accent);

  std::string mode;
  Color modeColor = palette::Dim;
  if (game.frozenTicks() > 0) {
    mode = TextFormat("FIREWALL %.1fs", static_cast<float>(game.frozenTicks()) / kTickRate);
    modeColor = palette::Core;
  } else if (game.mode() == Mode::Hunt) {
    mode = "HUNT";
    modeColor = palette::Danger;
  } else {
    mode = "PATROL";
  }
  DrawText(mode.c_str(), kCanvasW - 16 - MeasureText(mode.c_str(), 18), by + 15, 18, modeColor);
}

void Renderer::drawOverlayText(const Game& game) {
  const Vec2i exit = game.maze().spawns().keySpawn;
  const int y = static_cast<int>(tileCenter(exit).y) - 10;
  switch (game.phase()) {
    case Phase::Ready:
      drawTextCentered("READY", kCanvasW / 2, y, 22, palette::Warning);
      break;
    case Phase::LevelClear:
      drawTextCentered("SECTOR CLEAN", kCanvasW / 2, y, 22, palette::Accent);
      break;
    case Phase::GameOver:
      drawTextCentered("SYSTEM FAILURE", kCanvasW / 2, y, 22, palette::Danger);
      break;
    default: break;
  }
}

}  // namespace cr::app
