#pragma once

#include <gtest/gtest.h>

#include "core/Game.hpp"
#include "core/LevelLoader.hpp"

namespace cr::test {

// 11 x 8 maze with a tiny base. P = (5,1), core at (1,1), key at (9,1).
inline constexpr const char* kSmallMaze =
    "###########\n"
    "#o...P...S#\n"
    "#.###E###.#\n"
    "#.###-###.#\n"
    "#.#B123B#.#\n"
    "#.#######.#\n"
    "#.........#\n"
    "###########\n";

// Same maze but the bottom corridor wraps around the edges (tunnel). P = (4,6).
inline constexpr const char* kTunnelMaze =
    "###########\n"
    "#o.......S#\n"
    "#.###E###.#\n"
    "#.###-###.#\n"
    "#.#B123B#.#\n"
    "#.#######.#\n"
    "....P......\n"
    "###########\n";

inline Maze parseOrDie(const char* text) {
  std::string error;
  auto m = Maze::parse(text, &error);
  EXPECT_TRUE(m.has_value()) << error;
  return *m;
}

/// Locks every virus inside the base so tests can focus on one mechanic.
inline void parkViruses(Game& g) {
  for (VirusKind k : kAllVirusKinds) {
    Virus& v = g.mutableVirus(k);
    v.state = VirusState::InBase;
    v.releaseCountdown = 1'000'000;
    v.mover = Mover{};
    v.mover.placeAt(v.homeSlot);
  }
}

inline Game makeGame(const char* text, std::uint64_t seed = 1, int lives = 3) {
  GameOptions o;
  o.seed = seed;
  o.startLives = lives;
  o.levels = {parseOrDie(text)};
  Game g(o);
  g.forcePhase(Phase::Playing);
  parkViruses(g);
  g.drainEvents();
  return g;
}

inline void tickN(Game& g, int n, InputFrame in = {}) {
  for (int i = 0; i < n; ++i) g.tick(in);
}

inline std::vector<Maze> shippedLevels() {
  auto result = loadLevelsFromDirectory(CR_LEVEL_DIR);
  EXPECT_TRUE(result.errors.empty());
  return result.levels;
}

inline bool hasEvent(const std::vector<GameEvent>& events, EventType type) {
  for (const auto& e : events)
    if (e.type == type) return true;
  return false;
}

}  // namespace cr::test
