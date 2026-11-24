#include "TestUtil.hpp"
#include "core/MazeGenerator.hpp"

using namespace cr;
using namespace cr::test;

TEST(MazeGenerator, ProducesValidPlayableMazesForManySeeds) {
  for (std::uint64_t seed = 0; seed < 300; ++seed) {
    Maze m = MazeGenerator::generate(seed);  // throws if parsing/validation fails
    ASSERT_EQ(m.width(), MazeGenerator::kWidth);
    ASSERT_EQ(m.height(), MazeGenerator::kHeight);
    ASSERT_TRUE(allPickupsReachable(m)) << "seed " << seed;
    ASSERT_GT(m.totalPickups(), 150) << "seed " << seed;
  }
}

TEST(MazeGenerator, MazesAreHorizontallySymmetric) {
  for (std::uint64_t seed = 0; seed < 50; ++seed) {
    const Maze m = MazeGenerator::generate(seed);
    for (int y = 0; y < m.height(); ++y)
      for (int x = 0; x < m.width(); ++x)
        ASSERT_EQ(m.isWall({x, y}), m.isWall({m.width() - 1 - x, y})) << "seed " << seed;
  }
}

TEST(MazeGenerator, BorderIsClosedExceptForTheTunnel) {
  const Maze m = MazeGenerator::generate(99);
  for (int x = 0; x < m.width(); ++x) {
    EXPECT_TRUE(m.isWall({x, 0}));
    EXPECT_TRUE(m.isWall({x, m.height() - 1}));
  }
  for (int y = 0; y < m.height(); ++y) {
    if (y == 13) {
      EXPECT_TRUE(m.isOpen({0, y}));
    } else {
      EXPECT_TRUE(m.isWall({0, y})) << y;
    }
  }
}

TEST(MazeGenerator, IsDeterministicPerSeed) {
  EXPECT_EQ(MazeGenerator::generateText(1234), MazeGenerator::generateText(1234));
  EXPECT_NE(MazeGenerator::generateText(1234), MazeGenerator::generateText(1235));
}
