#include "core/LevelLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/Pathfinding.hpp"

namespace cr {

namespace fs = std::filesystem;

LevelLoadResult loadLevelsFromDirectory(const std::string& directory) {
  LevelLoadResult result;
  std::error_code ec;
  if (!fs::is_directory(directory, ec)) {
    result.errors.push_back("level directory not found: " + directory);
    return result;
  }
  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(directory, ec))
    if (entry.is_regular_file() && entry.path().extension() == ".txt") files.push_back(entry.path());
  std::sort(files.begin(), files.end());

  for (const auto& path : files) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string error;
    auto maze = Maze::parse(buffer.str(), &error);
    if (!maze) {
      result.errors.push_back(path.filename().string() + ": " + error);
      continue;
    }
    if (!allPickupsReachable(*maze)) {
      result.errors.push_back(path.filename().string() + ": some packets are unreachable");
      continue;
    }
    result.levels.push_back(std::move(*maze));
  }
  return result;
}

bool allPickupsReachable(const Maze& maze) {
  DistanceField field;
  field.compute(maze, maze.spawns().player, [&maze](Vec2i t) { return maze.isOpen(t); });
  for (int y = 0; y < maze.height(); ++y) {
    for (int x = 0; x < maze.width(); ++x) {
      const Tile t = maze.at({x, y});
      if ((t == Tile::Packet || t == Tile::Core) && !field.reachable({x, y})) return false;
    }
  }
  return field.reachable(maze.spawns().keySpawn);
}

}  // namespace cr
