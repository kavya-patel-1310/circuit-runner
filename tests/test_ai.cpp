#include "TestUtil.hpp"
#include "core/AutoPilot.hpp"
#include "core/Pathfinding.hpp"

using namespace cr;
using namespace cr::test;

namespace {
void enterHuntMode(Game& g) {
  tickN(g, g.config().schedule[0].ticks + 1);
  ASSERT_EQ(g.mode(), Mode::Hunt);
}
}  // namespace

TEST(VirusAI, PatrolModeTargetsHomeCorner) {
  Game g = makeGame(kSmallMaze);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  EXPECT_EQ(g.computeTarget(worm), worm.patrolCorner);
}

TEST(VirusAI, WormChasesThePlayerDirectly) {
  Game g = makeGame(kSmallMaze);
  enterHuntMode(g);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  EXPECT_EQ(g.computeTarget(worm), g.playerTile());
}

TEST(VirusAI, TrojanAimsAheadOfThePlayer) {
  Game g = makeGame(kTunnelMaze);
  enterHuntMode(g);
  g.tick({Dir::Right});
  Virus& trojan = g.mutableVirus(VirusKind::Trojan);
  trojan.state = VirusState::Active;
  EXPECT_EQ(g.computeTarget(trojan), (Vec2i{9, 6}));
}

TEST(VirusAI, PredictionFollowsCorridorsAroundCorners) {
  Game g = makeGame(kSmallMaze);  // player faces left by default
  EXPECT_EQ(g.predictPlayerTile(5), (Vec2i{1, 2}));
}

TEST(VirusAI, RootkitFlanksNearThePlayer) {
  GameOptions o;
  o.levels = shippedLevels();
  Game g(o);
  g.forcePhase(Phase::Playing);
  parkViruses(g);
  enterHuntMode(g);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  worm.mover.placeAt({1, 1});
  Virus& rootkit = g.mutableVirus(VirusKind::Rootkit);
  rootkit.state = VirusState::Active;
  rootkit.mover.placeAt({25, 27});

  const Vec2i target = g.computeTarget(rootkit);
  DistanceField fromPlayer, fromWorm;
  auto open = [&g](Vec2i t) { return g.maze().isOpen(t); };
  fromPlayer.compute(g.maze(), g.playerTile(), open);
  fromWorm.compute(g.maze(), Vec2i{1, 1}, open);
  EXPECT_GE(fromPlayer.at(target), 2);
  EXPECT_LE(fromPlayer.at(target), 6);
  // It should approach from the side away from the Worm.
  EXPECT_GT(fromWorm.at(target), fromWorm.at(g.playerTile()));
}

TEST(VirusAI, LeavingAndQuarantinedTargetsUseTheBase) {
  Game g = makeGame(kSmallMaze);
  Virus& v = g.mutableVirus(VirusKind::Trojan);
  v.state = VirusState::Leaving;
  EXPECT_EQ(g.computeTarget(v), g.maze().spawns().baseExit);
  v.state = VirusState::Quarantined;
  EXPECT_EQ(g.computeTarget(v), v.homeSlot);
}

TEST(VirusAI, VirusesLeaveTheBaseWhenReleased) {
  GameOptions o;
  o.levels = shippedLevels();
  Game g(o);
  AutoPilot pilot;  // keeps the player alive so the release timers run out
  std::array<bool, 4> escaped{};
  for (int i = 0; i < Game::kReadyTicks + 1200; ++i) {
    g.tick(pilot.decide(g));
    for (std::size_t k = 0; k < 4; ++k)
      if (g.viruses()[k].state == VirusState::Active) escaped[k] = true;
  }
  for (std::size_t k = 0; k < 4; ++k) EXPECT_TRUE(escaped[k]) << virusName(kAllVirusKinds[k]);
}

// Long-running soak test: the bot plays complete games while invariants are
// checked every tick.
TEST(AutoPilot, SoakTestPlaysFullGamesSafely) {
  const auto levels = shippedLevels();
  int totalScore = 0;
  int blinksChecked = 0;
  for (std::uint64_t seed = 1; seed <= 4; ++seed) {
    GameOptions o;
    o.seed = seed;
    o.levels = levels;
    Game g(o);
    AutoPilot pilot;
    DistanceField fromPlayer;
    for (int i = 0; i < 40000 && g.phase() != Phase::GameOver; ++i) {
      g.tick(pilot.decide(g));
      ASSERT_TRUE(g.maze().isOpen(g.playerTile())) << "player inside a wall";
      for (const auto& e : g.drainEvents()) {
        if (e.type != EventType::GlitchBlink) continue;
        fromPlayer.compute(g.maze(), g.playerTile(), [&g](Vec2i t) { return g.maze().isOpen(t); });
        const int d = fromPlayer.at(e.tile);
        EXPECT_GE(d, 8) << "Glitch blinked too close to the player";
        ++blinksChecked;
      }
    }
    totalScore += g.score();
  }
  EXPECT_GT(totalScore, 4 * 2000);  // the bot should clear a good chunk of each maze
  EXPECT_GT(blinksChecked, 0);
}

TEST(AutoPilot, HuntsFrozenVirusesForPoints) {
  GameOptions o;
  o.seed = 5;
  o.levels = shippedLevels();
  Game g(o);
  AutoPilot pilot;
  bool quarantined = false;
  for (int i = 0; i < 40000 && !quarantined && g.phase() != Phase::GameOver; ++i) {
    g.tick(pilot.decide(g));
    quarantined = hasEvent(g.drainEvents(), EventType::VirusQuarantined);
  }
  EXPECT_TRUE(quarantined);
}
