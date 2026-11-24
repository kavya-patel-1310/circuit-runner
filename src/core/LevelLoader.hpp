#pragma once

#include <string>
#include <vector>

#include "core/Maze.hpp"

namespace cr {

struct LevelLoadResult {
  std::vector<Maze> levels;
  std::vector<std::string> errors;  // one entry per file that failed to parse
};

/// Loads every *.txt in `directory`, sorted by filename.
LevelLoadResult loadLevelsFromDirectory(const std::string& directory);

/// True if every pickup is reachable from the player spawn.
bool allPickupsReachable(const Maze& maze);

}  // namespace cr
