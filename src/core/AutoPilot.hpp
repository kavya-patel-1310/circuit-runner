#pragma once

#include "core/Game.hpp"
#include "core/Pathfinding.hpp"

namespace cr {

/// A bot that plays the game. Powers the title-screen attract mode, the
/// headless simulator and the long-running soak tests.
///
/// Strategy: a multi-source BFS from every dangerous virus builds a "danger
/// field". Tiles within a safety margin are treated as walls, then a second
/// multi-source BFS from every remaining pickup finds the nearest safe goal.
/// When no safe route exists, it flees along the danger gradient.
class AutoPilot {
 public:
  explicit AutoPilot(int safetyMargin = 2) : safetyMargin_(safetyMargin) {}
  InputFrame decide(const Game& game);

 private:
  int safetyMargin_;
  DistanceField danger_;
  DistanceField goal_;
  std::vector<Vec2i> threats_;
  std::vector<Vec2i> goals_;
};

}  // namespace cr
