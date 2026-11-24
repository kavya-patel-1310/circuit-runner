#include "core/LevelConfig.hpp"

#include <algorithm>

#include "core/Types.hpp"

namespace cr {

LevelConfig makeLevelConfig(int level) {
  LevelConfig c;
  const int t = std::clamp(level - 1, 0, 8);

  c.playerSpeedPct = 80 + t * 2;  // 80 -> 96
  c.virusSpeedPct = 72 + t * 3;   // 72 -> 96
  c.freezeTicks = std::max(secondsToTicks(2.0), secondsToTicks(7.0) - t * 36);
  c.keyPoints = std::min(5000, 100 + 200 * (level - 1));
  c.releaseTicks = {0, 30, std::max(60, 240 - t * 20), std::max(120, 480 - t * 40)};

  const auto s = [](double sec) { return secondsToTicks(sec); };
  if (level <= 2) {
    c.schedule = {{Mode::Patrol, s(7)}, {Mode::Hunt, s(20)}, {Mode::Patrol, s(7)},
                  {Mode::Hunt, s(20)},  {Mode::Patrol, s(5)}, {Mode::Hunt, -1}};
  } else if (level <= 4) {
    c.schedule = {{Mode::Patrol, s(5)}, {Mode::Hunt, s(20)}, {Mode::Patrol, s(5)},
                  {Mode::Hunt, s(25)},  {Mode::Patrol, s(3)}, {Mode::Hunt, -1}};
  } else {
    c.schedule = {{Mode::Patrol, s(4)}, {Mode::Hunt, s(25)}, {Mode::Patrol, s(3)},
                  {Mode::Hunt, s(30)},  {Mode::Patrol, s(1)}, {Mode::Hunt, -1}};
  }

  if (level >= 4) {
    c.blinkMinTicks = s(6);
    c.blinkMaxTicks = s(10);
  }
  return c;
}

}  // namespace cr
