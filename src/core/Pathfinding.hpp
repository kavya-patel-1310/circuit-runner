#pragma once

#include <climits>
#include <span>
#include <vector>

#include "core/Maze.hpp"

namespace cr {

/// Breadth-first distance field over the maze grid (with edge wrapping).
///
/// Instead of the classic "pick the neighbour closest to the target in a
/// straight line" heuristic, every agent here uses true shortest-path
/// distances, so viruses never get stuck behind walls. A multi-source BFS also
/// gives "distance to the nearest threat" for the autopilot in one pass.
class DistanceField {
 public:
  static constexpr int kUnreachable = INT_MAX / 4;

  template <class Passable>
  void compute(const Maze& maze, std::span<const Vec2i> sources, Passable&& passable);

  template <class Passable>
  void compute(const Maze& maze, Vec2i source, Passable&& passable) {
    compute(maze, std::span<const Vec2i>(&source, 1), passable);
  }

  int at(Vec2i p) const {
    if (dist_.empty()) return kUnreachable;
    const Vec2i w{floorMod(p.x, width_), floorMod(p.y, height_)};
    return dist_[static_cast<std::size_t>(w.y * width_ + w.x)];
  }
  bool reachable(Vec2i p) const { return at(p) != kUnreachable; }

 private:
  int width_ = 0;
  int height_ = 0;
  std::vector<int> dist_;
  std::vector<int> queue_;
};

template <class Passable>
void DistanceField::compute(const Maze& maze, std::span<const Vec2i> sources, Passable&& passable) {
  width_ = maze.width();
  height_ = maze.height();
  dist_.assign(static_cast<std::size_t>(maze.cellCount()), kUnreachable);
  queue_.clear();
  queue_.reserve(static_cast<std::size_t>(maze.cellCount()));

  for (Vec2i s : sources) {
    const int i = maze.index(maze.wrap(s));
    if (dist_[static_cast<std::size_t>(i)] == kUnreachable) {
      dist_[static_cast<std::size_t>(i)] = 0;
      queue_.push_back(i);
    }
  }
  for (std::size_t head = 0; head < queue_.size(); ++head) {
    const int i = queue_[head];
    const Vec2i p{i % width_, i / width_};
    const int next = dist_[static_cast<std::size_t>(i)] + 1;
    for (Dir d : kAllDirs) {
      const Vec2i n = maze.wrap(p + delta(d));
      const auto j = static_cast<std::size_t>(maze.index(n));
      if (dist_[j] != kUnreachable || !passable(n)) continue;
      dist_[j] = next;
      queue_.push_back(static_cast<int>(j));
    }
  }
}

/// Picks the direction out of `from` that most reduces the distance in `field`.
/// U-turns are only taken if `allowReverse` is set or nothing else is possible.
/// If the target is unreachable, falls back to straight-line distance.
template <class Passable>
Dir chooseDirection(const Maze& maze, Vec2i from, Dir current, const DistanceField& field, Vec2i fallbackTarget,
                    bool allowReverse, Passable&& passable) {
  const Dir reverse = opposite(current);
  Dir best = Dir::None;
  long long bestScore = LLONG_MAX;
  for (int pass = 0; pass < 2 && best == Dir::None; ++pass) {
    for (Dir d : kAllDirs) {
      if (pass == 0 && !allowReverse && current != Dir::None && d == reverse) continue;
      const Vec2i n = maze.wrap(from + delta(d));
      if (!passable(n)) continue;
      const int dist = field.at(n);
      long long score;
      if (dist != DistanceField::kUnreachable) {
        score = dist;
      } else {
        const long long dx = n.x - fallbackTarget.x;
        const long long dy = n.y - fallbackTarget.y;
        score = 1'000'000 + dx * dx + dy * dy;
      }
      if (score < bestScore) {
        bestScore = score;
        best = d;
      }
    }
  }
  return best;
}

}  // namespace cr
