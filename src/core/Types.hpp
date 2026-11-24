#pragma once

#include <array>
#include <cstdint>

namespace cr {

/// Positions are stored in integer sub-tile units so the simulation is exactly
/// reproducible across machines (no floating-point drift). This is what makes
/// deterministic replays possible.
inline constexpr int kSub = 1200;
inline constexpr int kTickRate = 60;
/// Base movement speed: 8 tiles per second.
inline constexpr int kBaseSpeed = 8 * kSub / kTickRate;  // 160 sub-units per tick

enum class Dir : std::uint8_t { None, Up, Left, Down, Right };

/// Deterministic tie-break order used everywhere a direction is chosen.
inline constexpr std::array<Dir, 4> kAllDirs{Dir::Up, Dir::Left, Dir::Down, Dir::Right};

struct Vec2i {
  int x = 0;
  int y = 0;

  friend constexpr bool operator==(const Vec2i&, const Vec2i&) = default;
  friend constexpr Vec2i operator+(Vec2i a, Vec2i b) { return {a.x + b.x, a.y + b.y}; }
  friend constexpr Vec2i operator-(Vec2i a, Vec2i b) { return {a.x - b.x, a.y - b.y}; }
};

constexpr Vec2i delta(Dir d) {
  switch (d) {
    case Dir::Up: return {0, -1};
    case Dir::Down: return {0, 1};
    case Dir::Left: return {-1, 0};
    case Dir::Right: return {1, 0};
    case Dir::None: break;
  }
  return {0, 0};
}

constexpr Dir opposite(Dir d) {
  switch (d) {
    case Dir::Up: return Dir::Down;
    case Dir::Down: return Dir::Up;
    case Dir::Left: return Dir::Right;
    case Dir::Right: return Dir::Left;
    case Dir::None: break;
  }
  return Dir::None;
}

constexpr bool isHorizontal(Dir d) { return d == Dir::Left || d == Dir::Right; }

constexpr int floorDiv(int a, int b) {
  int q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

constexpr int floorMod(int a, int b) {
  int m = a % b;
  return m < 0 ? m + b : m;
}

/// Converts a speed percentage (100 = base speed) to sub-units per tick.
constexpr int scaledSpeed(int percent) { return kBaseSpeed * percent / 100; }

constexpr int secondsToTicks(double seconds) { return static_cast<int>(seconds * kTickRate); }

}  // namespace cr
