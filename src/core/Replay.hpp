#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Game.hpp"

namespace cr {

/// A recorded game: seed + level fingerprint + one InputFrame per tick.
/// Because the simulation is deterministic, this tiny file reproduces an
/// entire session exactly. Frames are run-length encoded on disk.
struct Replay {
  std::uint64_t seed = 0;
  int startLives = 3;
  std::uint64_t levelsHash = 0;
  std::vector<InputFrame> frames;

  std::string toString() const;
  static std::optional<Replay> fromString(std::string_view text, std::string* error = nullptr);

  bool save(const std::string& path) const;
  static std::optional<Replay> load(const std::string& path, std::string* error = nullptr);
};

/// Fingerprint of a level set, so a replay refuses to run on different levels.
std::uint64_t hashLevels(const std::vector<Maze>& levels);

struct ReplayResult {
  bool ok = false;
  std::string error;
  int score = 0;
  int level = 0;
  std::uint64_t ticks = 0;
  std::uint64_t finalHash = 0;
};

/// Re-simulates a replay headlessly.
ReplayResult runReplay(const Replay& replay, const std::vector<Maze>& levels);

}  // namespace cr
