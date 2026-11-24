#include "TestUtil.hpp"
#include "core/Pathfinding.hpp"

using namespace cr;
using namespace cr::test;

namespace {
auto openFor(const Maze& m) {
  return [&m](Vec2i t) { return m.isOpen(t); };
}
}  // namespace

TEST(Pathfinding, BfsComputesShortestDistances) {
  const Maze m = parseOrDie(kSmallMaze);
  DistanceField f;
  f.compute(m, m.spawns().player, openFor(m));
  EXPECT_EQ(f.at({5, 1}), 0);
  EXPECT_EQ(f.at({9, 1}), 4);
  EXPECT_EQ(f.at({1, 6}), 9);
  EXPECT_EQ(f.at({5, 6}), 13);  // around either side
}

TEST(Pathfinding, BaseIsUnreachableForPlayer) {
  const Maze m = parseOrDie(kSmallMaze);
  DistanceField f;
  f.compute(m, m.spawns().player, openFor(m));
  EXPECT_FALSE(f.reachable({5, 4}));
  EXPECT_FALSE(f.reachable({0, 0}));
}

TEST(Pathfinding, BaseTrafficCanPassTheGate) {
  const Maze m = parseOrDie(kSmallMaze);
  DistanceField f;
  f.compute(m, m.spawns().baseSlots[1], [&m](Vec2i t) { return m.isOpenForBaseTraffic(t); });
  EXPECT_EQ(f.at(m.spawns().baseExit), 2);
}

TEST(Pathfinding, MultiSourceTakesNearest) {
  const Maze m = parseOrDie(kSmallMaze);
  DistanceField f;
  const std::vector<Vec2i> sources{{1, 1}, {9, 6}};
  f.compute(m, sources, openFor(m));
  EXPECT_EQ(f.at({1, 6}), 5);
  EXPECT_EQ(f.at({9, 1}), 5);
  EXPECT_EQ(f.at({5, 1}), 4);
}

TEST(Pathfinding, DistancesWrapThroughTunnels) {
  const Maze m = parseOrDie(kTunnelMaze);
  DistanceField f;
  f.compute(m, Vec2i{0, 6}, openFor(m));
  EXPECT_EQ(f.at({10, 6}), 1);
}

TEST(Pathfinding, ChooseDirectionFollowsGradientWithoutUTurns) {
  const Maze m = parseOrDie(kSmallMaze);
  DistanceField f;
  f.compute(m, Vec2i{1, 6}, openFor(m));
  // From the player, going left is shortest.
  EXPECT_EQ(chooseDirection(m, {5, 1}, Dir::None, f, {1, 6}, false, openFor(m)), Dir::Left);
  // Moving right: a U-turn is forbidden, so it must keep going right.
  EXPECT_EQ(chooseDirection(m, {7, 1}, Dir::Right, f, {1, 6}, false, openFor(m)), Dir::Right);
  // ...unless reversing is explicitly allowed.
  EXPECT_EQ(chooseDirection(m, {7, 1}, Dir::Right, f, {1, 6}, true, openFor(m)), Dir::Left);
}

TEST(Pathfinding, ReversesOnlyAtDeadEnds) {
  const Maze m = parseOrDie(
      "#######\n"
      "#P...S#\n"
      "##E####\n"
      "##-####\n"
      "#1.23##\n"
      "#######\n");
  DistanceField f;
  f.compute(m, Vec2i{1, 1}, openFor(m));
  EXPECT_EQ(chooseDirection(m, {5, 1}, Dir::Right, f, {1, 1}, false, openFor(m)), Dir::Left);
}
