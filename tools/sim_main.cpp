// Headless simulator: runs the AutoPilot through many games without a window.
// Useful for balancing, benchmarking and verifying replays.
//
//   circuit_runner_sim --games 20 --seed 42 --levels assets/levels
//   circuit_runner_sim --verify-replay last_replay.crr --levels assets/levels

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/AutoPilot.hpp"
#include "core/Game.hpp"
#include "core/LevelLoader.hpp"
#include "core/Replay.hpp"

namespace {

void usage() {
  std::puts(
      "usage: circuit_runner_sim [--games N] [--seed S] [--levels DIR]\n"
      "                          [--max-ticks T] [--record FILE] [--verify-replay FILE]");
}

}  // namespace

int main(int argc, char** argv) {
  int games = 10;
  std::uint64_t seed = 1;
  std::string levelDir = "assets/levels";
  std::uint64_t maxTicks = 60ull * 60 * 30;  // 30 minutes of game time
  std::string recordPath, verifyPath;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        usage();
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--games") games = std::stoi(next());
    else if (a == "--seed") seed = std::stoull(next());
    else if (a == "--levels") levelDir = next();
    else if (a == "--max-ticks") maxTicks = std::stoull(next());
    else if (a == "--record") recordPath = next();
    else if (a == "--verify-replay") verifyPath = next();
    else {
      usage();
      return a == "--help" ? 0 : 2;
    }
  }

  auto loaded = cr::loadLevelsFromDirectory(levelDir);
  for (const auto& e : loaded.errors) std::fprintf(stderr, "warning: %s\n", e.c_str());

  if (!verifyPath.empty()) {
    std::string error;
    auto replay = cr::Replay::load(verifyPath, &error);
    if (!replay) {
      std::fprintf(stderr, "error: %s\n", error.c_str());
      return 1;
    }
    const auto r = cr::runReplay(*replay, loaded.levels);
    if (!r.ok) {
      std::fprintf(stderr, "error: %s\n", r.error.c_str());
      return 1;
    }
    std::printf("replay OK: score=%d level=%d ticks=%llu hash=%016llx\n", r.score, r.level,
                static_cast<unsigned long long>(r.ticks), static_cast<unsigned long long>(r.finalHash));
    return 0;
  }

  std::printf("%-6s %-10s %-8s %-10s %-10s\n", "game", "seed", "level", "score", "ticks");
  long long totalScore = 0, totalTicks = 0;
  int bestLevel = 0;
  const auto start = std::chrono::steady_clock::now();

  for (int g = 0; g < games; ++g) {
    cr::GameOptions options;
    options.seed = seed + static_cast<std::uint64_t>(g);
    options.levels = loaded.levels;
    cr::Game game(options);
    cr::AutoPilot pilot;
    cr::Replay replay{options.seed, options.startLives, cr::hashLevels(loaded.levels), {}};

    while (game.phase() != cr::Phase::GameOver && game.tickCount() < maxTicks) {
      const cr::InputFrame input = pilot.decide(game);
      if (!recordPath.empty() && g == 0) replay.frames.push_back(input);
      game.tick(input);
      game.drainEvents();
    }
    if (!recordPath.empty() && g == 0) replay.save(recordPath);

    std::printf("%-6d %-10llu %-8d %-10d %-10llu\n", g + 1, static_cast<unsigned long long>(options.seed), game.level(),
                game.score(), static_cast<unsigned long long>(game.tickCount()));
    totalScore += game.score();
    totalTicks += static_cast<long long>(game.tickCount());
    if (game.level() > bestLevel) bestLevel = game.level();
  }

  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("\naverage score %.0f | best level %d | %.0f ticks/sec (%.0fx real time)\n",
              static_cast<double>(totalScore) / games, bestLevel, static_cast<double>(totalTicks) / secs,
              static_cast<double>(totalTicks) / secs / cr::kTickRate);
  return 0;
}
