#include "TestUtil.hpp"
#include "core/AutoPilot.hpp"

using namespace cr;
using namespace cr::test;

TEST(Game, StartsInReadyPhaseThenPlays) {
  GameOptions o;
  o.levels = {parseOrDie(kSmallMaze)};
  Game g(o);
  EXPECT_EQ(g.phase(), Phase::Ready);
  tickN(g, Game::kReadyTicks);
  EXPECT_EQ(g.phase(), Phase::Playing);
}

TEST(Game, EatingAPacketScores) {
  Game g = makeGame(kTunnelMaze);
  tickN(g, 12, {Dir::Right});
  EXPECT_EQ(g.score(), Game::kPacketPoints);
  EXPECT_EQ(g.maze().at({5, 6}), Tile::Empty);
  EXPECT_TRUE(hasEvent(g.drainEvents(), EventType::PacketEaten));
}

TEST(Game, CoreFreezesActiveViruses) {
  Game g = makeGame(kSmallMaze);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  worm.mover.placeAt({9, 6});
  tickN(g, 45, {Dir::Left});  // reach the core at (1,1)
  EXPECT_EQ(g.virus(VirusKind::Worm).state, VirusState::Frozen);
  EXPECT_GT(g.frozenTicks(), 0);
  EXPECT_TRUE(hasEvent(g.drainEvents(), EventType::CoreEaten));
}

TEST(Game, FreezeWearsOff) {
  Game g = makeGame(kSmallMaze);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  worm.mover.placeAt({9, 6});
  tickN(g, 45, {Dir::Left});
  tickN(g, g.config().freezeTicks + 1);
  EXPECT_EQ(g.frozenTicks(), 0);
  EXPECT_EQ(g.virus(VirusKind::Worm).state, VirusState::Active);
}

TEST(Game, QuarantineComboDoublesPoints) {
  Game g = makeGame(kSmallMaze);
  tickN(g, 45, {Dir::Left});  // eat the core; player now parked at (1,1)
  ASSERT_GT(g.frozenTicks(), 0);
  g.drainEvents();

  const std::array<VirusKind, 3> order{VirusKind::Worm, VirusKind::Trojan, VirusKind::Rootkit};
  int expected = g.score();
  int points = Game::kVirusBasePoints;
  for (VirusKind k : order) {
    Virus& v = g.mutableVirus(k);
    v.state = VirusState::Frozen;
    v.mover = g.player();
    g.tick({});
    expected += points;
    EXPECT_EQ(g.score(), expected);
    EXPECT_EQ(g.virus(k).state, VirusState::Quarantined);
    points *= 2;
  }
}

TEST(Game, QuarantinedVirusReturnsToBaseAndRespawns) {
  Game g = makeGame(kSmallMaze);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Quarantined;
  worm.releaseCountdown = 0;
  worm.mover.placeAt({9, 6});
  bool backInBase = false;
  for (int i = 0; i < 600 && !backInBase; ++i) {
    g.tick({});
    backInBase = g.virus(VirusKind::Worm).state == VirusState::InBase ||
                 g.virus(VirusKind::Worm).state == VirusState::Leaving;
  }
  EXPECT_TRUE(backInBase);
}

TEST(Game, TouchingAnActiveVirusCostsALife) {
  Game g = makeGame(kSmallMaze);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  worm.mover = g.player();
  g.tick({});
  EXPECT_EQ(g.phase(), Phase::Dying);
  EXPECT_TRUE(hasEvent(g.drainEvents(), EventType::PlayerCaught));
  tickN(g, Game::kDyingTicks);
  EXPECT_EQ(g.lives(), 2);
  EXPECT_EQ(g.phase(), Phase::Ready);
  EXPECT_EQ(g.player().px, g.maze().spawns().player.x * kSub);  // actors reset
}

TEST(Game, LosingTheLastLifeEndsTheGame) {
  Game g = makeGame(kSmallMaze, 1, /*lives=*/1);
  Virus& worm = g.mutableVirus(VirusKind::Worm);
  worm.state = VirusState::Active;
  worm.mover = g.player();
  tickN(g, Game::kDyingTicks + 1);
  EXPECT_EQ(g.phase(), Phase::GameOver);
  EXPECT_TRUE(hasEvent(g.drainEvents(), EventType::GameOver));
}

TEST(Game, ClearingAllPickupsAdvancesToAProceduralLevel) {
  Game g = makeGame(kTunnelMaze);
  Maze& m = g.mutableMaze();
  for (int y = 0; y < m.height(); ++y)
    for (int x = 0; x < m.width(); ++x)
      if (m.at({x, y}) == Tile::Packet || m.at({x, y}) == Tile::Core) m.set({x, y}, Tile::Empty);
  m.set({5, 6}, Tile::Packet);
  tickN(g, 12, {Dir::Right});
  EXPECT_EQ(g.phase(), Phase::LevelClear);
  tickN(g, Game::kLevelClearTicks);
  EXPECT_EQ(g.level(), 2);
  EXPECT_EQ(g.phase(), Phase::Ready);
  EXPECT_EQ(g.maze().width(), 27);  // generated level
}

TEST(Game, ExtraLifeAtScoreThreshold) {
  GameOptions o;
  o.levels = {parseOrDie(kTunnelMaze)};
  o.extraLifeEvery = 20;
  Game g(o);
  g.forcePhase(Phase::Playing);
  parkViruses(g);
  tickN(g, 25, {Dir::Right});  // two packets
  EXPECT_EQ(g.lives(), 4);
  EXPECT_TRUE(hasEvent(g.drainEvents(), EventType::ExtraLife));
}

TEST(Game, KeySpawnsAndCanBeCollected) {
  GameOptions o;
  o.seed = 3;
  o.levels = shippedLevels();
  Game g(o);
  AutoPilot pilot;
  bool spawned = false;
  for (int i = 0; i < 20000 && !spawned && g.phase() != Phase::GameOver; ++i) {
    g.tick(pilot.decide(g));
    spawned = hasEvent(g.drainEvents(), EventType::KeySpawned);
  }
  ASSERT_TRUE(spawned);
  EXPECT_TRUE(g.keyActive());
  const int before = g.score();
  g.mutablePlayer().placeAt(g.keyTile());
  parkViruses(g);
  g.tick({});
  EXPECT_FALSE(g.keyActive());
  EXPECT_EQ(g.score(), before + g.config().keyPoints);
}

TEST(Game, ModeScheduleAlternatesPatrolAndHunt) {
  Game g = makeGame(kSmallMaze);
  EXPECT_EQ(g.mode(), Mode::Patrol);
  tickN(g, g.config().schedule[0].ticks + 1);
  EXPECT_EQ(g.mode(), Mode::Hunt);
}

TEST(Game, IsDeterministicForTheSameSeed) {
  auto run = [](std::uint64_t seed) {
    GameOptions o;
    o.seed = seed;
    o.levels = shippedLevels();
    Game g(o);
    AutoPilot pilot;
    for (int i = 0; i < 4000; ++i) g.tick(pilot.decide(g));
    return g.stateHash();
  };
  EXPECT_EQ(run(11), run(11));
  EXPECT_NE(run(11), run(12));
}
