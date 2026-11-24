#include "core/AutoPilot.hpp"

namespace cr {

InputFrame AutoPilot::decide(const Game& game) {
  InputFrame out;
  if (game.phase() != Phase::Playing) return out;

  const Maze& maze = game.maze();
  auto open = [&maze](Vec2i t) { return maze.isOpen(t); };
  const Vec2i me = game.playerTile();

  threats_.clear();
  goals_.clear();
  for (const Virus& v : game.viruses()) {
    const Vec2i t = maze.wrap(v.mover.tile());
    if (v.dangerous()) threats_.push_back(t);
    else if (v.catchable() && game.frozenTicks() > 60) goals_.push_back(t);  // hunt frozen viruses
  }

  danger_.compute(maze, threats_, open);
  auto safe = [&](Vec2i t) { return open(t) && danger_.at(t) > safetyMargin_; };

  if (goals_.empty()) {
    if (game.keyActive()) goals_.push_back(game.keyTile());
    for (int y = 0; y < maze.height(); ++y) {
      for (int x = 0; x < maze.width(); ++x) {
        const Tile t = maze.at({x, y});
        if (t == Tile::Packet || t == Tile::Core) goals_.push_back({x, y});
      }
    }
  }
  goal_.compute(maze, goals_, safe);

  Dir best = Dir::None;
  int bestDist = DistanceField::kUnreachable;
  for (Dir d : kAllDirs) {
    const Vec2i n = maze.wrap(me + delta(d));
    if (!safe(n)) continue;
    const int dist = goal_.at(n);
    if (dist < bestDist) {
      bestDist = dist;
      best = d;
    }
  }

  if (best == Dir::None) {
    // Cornered: move to the neighbour farthest from any threat.
    int bestDanger = -1;
    for (Dir d : kAllDirs) {
      const Vec2i n = maze.wrap(me + delta(d));
      if (!open(n)) continue;
      const int dd = danger_.at(n);
      if (dd > bestDanger) {
        bestDanger = dd;
        best = d;
      }
    }
  }

  out.dir = best;
  out.overclock = game.overclockMeter() >= Game::kOverclockCost && !threats_.empty() && danger_.at(me) <= 5;
  return out;
}

}  // namespace cr
