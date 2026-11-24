#pragma once

#include <cstdint>
#include <string>

#include "core/Maze.hpp"

namespace cr {

/// Procedurally generates endless, symmetric, fully-connected levels.
///
/// Algorithm:
///  1. Randomised depth-first search carves a perfect maze over the left half.
///  2. "Braiding" removes every dead end and a few extra walls add loops, so
///     the player always has an escape route (pure perfect mazes are unfair in
///     a chase game).
///  3. The half is mirrored, the virus base and side tunnel are stamped in.
///  4. A BFS repair pass knocks through walls until every tile is reachable.
class MazeGenerator {
 public:
  static constexpr int kWidth = 27;
  static constexpr int kHeight = 29;

  static std::string generateText(std::uint64_t seed);
  static Maze generate(std::uint64_t seed);
};

}  // namespace cr
