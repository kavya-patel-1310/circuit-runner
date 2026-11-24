#include "TestUtil.hpp"

using namespace cr;
using namespace cr::test;

TEST(Maze, ParsesLegendAndSpawns) {
  const Maze m = parseOrDie(kSmallMaze);
  EXPECT_EQ(m.width(), 11);
  EXPECT_EQ(m.height(), 8);
  EXPECT_EQ(m.spawns().player, (Vec2i{5, 1}));
  EXPECT_EQ(m.spawns().baseExit, (Vec2i{5, 2}));
  EXPECT_EQ(m.spawns().keySpawn, (Vec2i{9, 1}));
  EXPECT_EQ(m.spawns().baseSlots[0], (Vec2i{4, 4}));
  EXPECT_EQ(m.spawns().baseSlots[2], (Vec2i{6, 4}));
  EXPECT_EQ(m.at({1, 1}), Tile::Core);
  EXPECT_EQ(m.at({5, 3}), Tile::Gate);
  EXPECT_TRUE(m.isBase({3, 4}));
  EXPECT_FALSE(m.isOpen({5, 3}));               // gate blocks the player
  EXPECT_TRUE(m.isOpenForBaseTraffic({5, 3}));  // ...but not viruses going home
  EXPECT_EQ(m.pickupsRemaining(), m.totalPickups());
}

TEST(Maze, ReadsNameMetadata) {
  const Maze m = parseOrDie((std::string("; name: Test Grid\n") + kSmallMaze).c_str());
  EXPECT_EQ(m.name(), "Test Grid");
}

TEST(Maze, RejectsMissingPlayer) {
  std::string text = kSmallMaze;
  text[text.find('P')] = '.';
  std::string error;
  EXPECT_FALSE(Maze::parse(text, &error));
  EXPECT_NE(error.find("'P'"), std::string::npos);
}

TEST(Maze, RejectsUnknownCharacterWithLocation) {
  std::string text = kSmallMaze;
  text[text.find('S')] = '?';
  std::string error;
  EXPECT_FALSE(Maze::parse(text, &error));
  EXPECT_NE(error.find("row 2"), std::string::npos) << error;
}

TEST(Maze, RejectsDuplicateSlot) {
  std::string text = kSmallMaze;
  text[text.find('3')] = '1';
  EXPECT_FALSE(Maze::parse(text));
}

TEST(Maze, PadsRowsWithStrippedTrailingWhitespace) {
  std::string text = kSmallMaze;
  text.replace(text.find("#.........#\n"), 12, "#.........\n");  // last wall char dropped
  const Maze m = parseOrDie(text.c_str());
  EXPECT_EQ(m.width(), 11);
  EXPECT_EQ(m.at({10, 6}), Tile::Empty);
}

TEST(Maze, SerializeRoundTrips) {
  const Maze a = parseOrDie(kSmallMaze);
  const Maze b = parseOrDie(a.serialize().c_str());
  EXPECT_EQ(a.contentHash(), b.contentHash());
  EXPECT_EQ(a.serialize(), b.serialize());
}

TEST(Maze, SetTracksPickupCount) {
  Maze m = parseOrDie(kSmallMaze);
  const int before = m.pickupsRemaining();
  m.set({2, 1}, Tile::Empty);
  EXPECT_EQ(m.pickupsRemaining(), before - 1);
  m.set({2, 1}, Tile::Packet);
  EXPECT_EQ(m.pickupsRemaining(), before);
}

TEST(Maze, CoordinatesWrap) {
  const Maze m = parseOrDie(kTunnelMaze);
  EXPECT_EQ(m.wrap({-1, 6}), (Vec2i{10, 6}));
  EXPECT_EQ(m.wrap({11, 6}), (Vec2i{0, 6}));
  EXPECT_TRUE(m.isOpen({-1, 6}));
}

TEST(Maze, ShippedLevelsAreValidAndSymmetric) {
  const auto levels = shippedLevels();
  ASSERT_GE(levels.size(), 2u);
  for (const Maze& m : levels) {
    EXPECT_TRUE(allPickupsReachable(m)) << m.name();
    for (int y = 0; y < m.height(); ++y)
      for (int x = 0; x < m.width(); ++x)
        EXPECT_EQ(m.isWall({x, y}), m.isWall({m.width() - 1 - x, y})) << m.name() << " at " << x << "," << y;
  }
}
