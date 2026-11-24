#pragma once

#include <algorithm>
#include <cstdlib>

#include "core/Maze.hpp"

namespace cr {

/// Grid-locked movement in integer sub-tile units. Turns happen only at tile
/// centres; reversing is allowed anywhere.
struct Mover {
  int px = 0;  // absolute position in sub-units; tile centre = tile * kSub
  int py = 0;
  Dir dir = Dir::None;

  void placeAt(Vec2i t) {
    px = t.x * kSub;
    py = t.y * kSub;
  }
  bool atCenter() const { return px % kSub == 0 && py % kSub == 0; }
  /// Nearest tile (unwrapped — call Maze::wrap on the result).
  Vec2i tile() const { return {floorDiv(px + kSub / 2, kSub), floorDiv(py + kSub / 2, kSub)}; }

  int distanceToNextCenter() const {
    const int r = isHorizontal(dir) ? px % kSub : py % kSub;
    if (r == 0) return kSub;
    return (dir == Dir::Right || dir == Dir::Down) ? kSub - r : r;
  }
};

/// Advances `m` by up to `budget` sub-units. Every time a tile centre is
/// reached, `onCenter(tile)` runs (e.g. eat a packet) and then
/// `decide(mover, tile)` may change direction. Movement stops at walls.
template <class OnCenter, class Decide, class CanEnter>
void advance(Mover& m, int budget, const Maze& maze, OnCenter&& onCenter, Decide&& decide, CanEnter&& canEnter) {
  const int wrapX = maze.width() * kSub;
  const int wrapY = maze.height() * kSub;
  for (int guard = 0; budget > 0 && guard < 16; ++guard) {
    if (m.atCenter()) {
      const Vec2i here = maze.wrap(m.tile());
      onCenter(here);
      decide(m, here);
      if (m.dir == Dir::None) return;
      if (!canEnter(maze.wrap(here + delta(m.dir)))) {
        m.dir = Dir::None;
        return;
      }
    } else if (m.dir == Dir::None) {
      return;
    }
    const int step = std::min(budget, m.distanceToNextCenter());
    const Vec2i d = delta(m.dir);
    m.px = floorMod(m.px + d.x * step, wrapX);
    m.py = floorMod(m.py + d.y * step, wrapY);
    budget -= step;
  }
}

/// Manhattan distance between two movers in sub-units, respecting wrap.
inline int wrappedDistance(const Mover& a, const Mover& b, const Maze& maze) {
  const int w = maze.width() * kSub;
  const int h = maze.height() * kSub;
  int dx = std::abs(a.px - b.px);
  int dy = std::abs(a.py - b.py);
  dx = std::min(dx, w - dx);
  dy = std::min(dy, h - dy);
  return dx + dy;
}

}  // namespace cr
