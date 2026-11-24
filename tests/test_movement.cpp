#include "TestUtil.hpp"

using namespace cr;
using namespace cr::test;

namespace {
constexpr int kPlayerStep = scaledSpeed(80);  // level 1 player speed per tick
}

TEST(Movement, PlayerMovesAtConfiguredSpeed) {
  Game g = makeGame(kTunnelMaze);
  tickN(g, 10, {Dir::Right});
  EXPECT_EQ(g.player().px, 4 * kSub + 10 * kPlayerStep);
  EXPECT_EQ(g.player().py, 6 * kSub);
}

TEST(Movement, PlayerStopsAtWalls) {
  Game g = makeGame(kSmallMaze);
  tickN(g, 5, {Dir::Up});
  EXPECT_EQ(g.player().px, 5 * kSub);
  EXPECT_EQ(g.player().py, 1 * kSub);
  EXPECT_EQ(g.player().dir, Dir::None);
}

TEST(Movement, BufferedTurnIsTakenAtNextJunction) {
  Game g = makeGame(kSmallMaze);
  g.tick({Dir::Left});
  tickN(g, 49, {Dir::Down});  // Down is blocked until the corner at x = 1
  EXPECT_EQ(g.player().px, 1 * kSub);
  EXPECT_EQ(g.player().py, 1 * kSub + (50 * kPlayerStep - 4 * kSub));
}

TEST(Movement, ReversingIsInstantMidTile) {
  Game g = makeGame(kTunnelMaze);
  tickN(g, 3, {Dir::Right});
  g.tick({Dir::Left});
  EXPECT_EQ(g.player().px, 4 * kSub + 2 * kPlayerStep);
  EXPECT_EQ(g.player().dir, Dir::Left);
}

TEST(Movement, TunnelWrapsToTheOtherSide) {
  Game g = makeGame(kTunnelMaze);
  tickN(g, 50, {Dir::Left});
  EXPECT_EQ(g.player().px, floorMod(4 * kSub - 50 * kPlayerStep, 11 * kSub));
  EXPECT_EQ(g.playerTile(), (Vec2i{10, 6}));
}

TEST(Movement, OverclockBoostsSpeed) {
  Game g = makeGame(kTunnelMaze);
  // Fill the meter by hand: eat 50 packets' worth.
  for (int i = 0; i < Game::kOverclockCost / Game::kOverclockPerPickup; ++i) {
    g.mutableMaze().set({4, 6}, Tile::Packet);
    g.tick({});
  }
  ASSERT_EQ(g.overclockMeter(), Game::kOverclockCost);
  g.tick({Dir::Right, true});
  EXPECT_EQ(g.overclockMeter(), 0);
  EXPECT_GT(g.overclockTicks(), 0);
  EXPECT_EQ(g.player().px, 4 * kSub + scaledSpeed(80 + 35));
}

TEST(Movement, OverclockNeedsAFullMeter) {
  Game g = makeGame(kTunnelMaze);
  g.tick({Dir::Right, true});
  EXPECT_EQ(g.overclockTicks(), 0);
}

TEST(Movement, VirusesNeverLeaveWalkableTiles) {
  GameOptions o;
  o.seed = 7;
  o.levels = shippedLevels();
  Game g(o);
  for (int i = 0; i < 3000; ++i) {
    g.tick({});
    for (const Virus& v : g.viruses()) {
      const Vec2i t = g.maze().wrap(v.mover.tile());
      ASSERT_TRUE(g.maze().isOpenForBaseTraffic(t)) << virusName(v.kind) << " in a wall at tick " << i;
      // Movers are always aligned to the grid on the perpendicular axis.
      ASSERT_TRUE(v.mover.px % kSub == 0 || v.mover.py % kSub == 0);
    }
  }
}
