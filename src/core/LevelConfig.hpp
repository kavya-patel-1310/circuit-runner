#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace cr {

enum class Mode : std::uint8_t { Patrol, Hunt };

struct ModePhase {
  Mode mode;
  int ticks;  // < 0 means "until the level ends"
};

/// All difficulty tuning lives here as data, so balancing never touches logic.
struct LevelConfig {
  int playerSpeedPct = 80;
  int virusSpeedPct = 72;
  int frozenSpeedPct = 50;
  int leavingSpeedPct = 50;
  int quarantinedSpeedPct = 200;
  int overclockBonusPct = 35;
  int freezeTicks = 420;
  int freezeWarningTicks = 120;
  int keyPoints = 100;
  int keyTicks = 540;
  std::array<int, 4> releaseTicks{0, 30, 240, 480};
  std::vector<ModePhase> schedule;
  int blinkMinTicks = 540;
  int blinkMaxTicks = 840;
};

/// Difficulty curve: everything ramps up until level 9, then plateaus.
LevelConfig makeLevelConfig(int level);

}  // namespace cr
