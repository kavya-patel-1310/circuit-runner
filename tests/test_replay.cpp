#include <algorithm>
#include "TestUtil.hpp"
#include "core/AutoPilot.hpp"
#include "core/Replay.hpp"

using namespace cr;
using namespace cr::test;

namespace {
struct Recorded {
  Replay replay;
  std::uint64_t hash;
  int score;
};

Recorded recordBotGame(const std::vector<Maze>& levels, std::uint64_t seed, int ticks) {
  GameOptions o;
  o.seed = seed;
  o.levels = levels;
  Game g(o);
  AutoPilot pilot;
  Replay r{seed, o.startLives, hashLevels(levels), {}};
  for (int i = 0; i < ticks; ++i) {
    const InputFrame in = pilot.decide(g);
    r.frames.push_back(in);
    g.tick(in);
  }
  return {r, g.stateHash(), g.score()};
}
}  // namespace

TEST(Replay, ReproducesTheExactGame) {
  const auto levels = shippedLevels();
  const Recorded rec = recordBotGame(levels, 21, 5000);
  const auto parsed = Replay::fromString(rec.replay.toString());
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->frames.size(), rec.replay.frames.size());
  const ReplayResult result = runReplay(*parsed, levels);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.finalHash, rec.hash);
  EXPECT_EQ(result.score, rec.score);
}

TEST(Replay, RunLengthEncodingIsCompact) {
  const auto levels = shippedLevels();
  const Recorded rec = recordBotGame(levels, 2, 5000);
  const std::string text = rec.replay.toString();
  // Far fewer lines than frames: inputs repeat for long stretches.
  const auto lines = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
  EXPECT_LT(lines, rec.replay.frames.size() / 3);
}

TEST(Replay, RefusesADifferentLevelSet) {
  const auto levels = shippedLevels();
  const Recorded rec = recordBotGame(levels, 3, 100);
  std::vector<Maze> other{levels.at(1), levels.at(0)};
  const ReplayResult result = runReplay(rec.replay, other);
  EXPECT_FALSE(result.ok);
}

TEST(Replay, RejectsCorruptFiles) {
  EXPECT_FALSE(Replay::fromString("hello world"));
  EXPECT_FALSE(Replay::fromString("CIRCUITRUNNER-REPLAY 1\nseed 1\nlives 3\nlevels ff\nruns 1\n5 9 0\n"));
  EXPECT_FALSE(Replay::fromString("CIRCUITRUNNER-REPLAY 99\n"));
}
