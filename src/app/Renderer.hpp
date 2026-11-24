#pragma once

#include <string>
#include <vector>

#include "core/Game.hpp"
#include "raylib.h"

namespace cr::app {

/// Virtual canvas size; the window scales it with letterboxing.
inline constexpr int kCanvasW = 648;
inline constexpr int kCanvasH = 800;
inline constexpr int kHudTop = 56;
inline constexpr int kHudBottom = 48;

namespace palette {
inline constexpr Color Background{8, 14, 26, 255};
inline constexpr Color GridLine{14, 26, 44, 255};
inline constexpr Color WallFill{16, 44, 64, 255};
inline constexpr Color WallEdge{46, 230, 166, 255};
inline constexpr Color Packet{120, 220, 255, 255};
inline constexpr Color Core{255, 92, 205, 255};
inline constexpr Color Gate{255, 170, 60, 255};
inline constexpr Color Drone{125, 249, 255, 255};
inline constexpr Color Text{220, 240, 255, 255};
inline constexpr Color Dim{110, 140, 170, 255};
inline constexpr Color Accent{46, 230, 166, 255};
inline constexpr Color Warning{255, 196, 70, 255};
inline constexpr Color Danger{255, 77, 109, 255};
inline constexpr Color Frozen{170, 182, 196, 255};
}  // namespace palette

Color virusColor(VirusKind kind);

struct DrawOptions {
  bool showTargets = false;  // AI debug overlay (F1)
  bool dimmed = false;       // attract mode behind the title menu
  int highScore = 0;
};

/// Draws the simulation state. Owns purely cosmetic state (particles,
/// floating score text, screen shake) that never feeds back into the game.
class Renderer {
 public:
  void onEvent(const GameEvent& e, const Game& game);
  void update(float dt);
  void clearEffects();
  void draw(const Game& game, const DrawOptions& options);

  // Shared drawing helpers (also used by menus).
  static void drawDrone(Vector2 c, float r, Dir facing, float time, float alpha = 1.0f);
  static void drawVirusShape(VirusKind kind, Vector2 c, float r, float time, Color color, Dir facing);
  static void drawTextCentered(const std::string& text, int cx, int y, int size, Color color);

 private:
  struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;
    float maxLife;
    float size;
    Color color;
  };
  struct FloatingText {
    Vector2 pos;
    std::string text;
    float life;
    Color color;
  };

  void layout(const Maze& maze);
  Vector2 tileCenter(Vec2i t) const;
  Vector2 moverCenter(const Mover& m) const;
  void burst(Vector2 at, Color color, int count, float speed, float life);
  void drawMaze(const Game& game);
  void drawPickups(const Game& game);
  void drawViruses(const Game& game, const DrawOptions& options);
  void drawPlayer(const Game& game);
  void drawHud(const Game& game, const DrawOptions& options);
  void drawOverlayText(const Game& game);

  float tile_ = 24.0f;
  Vector2 origin_{0, 0};
  float time_ = 0.0f;
  float shake_ = 0.0f;
  float flash_ = 0.0f;
  std::vector<Particle> particles_;
  std::vector<FloatingText> texts_;
};

}  // namespace cr::app
