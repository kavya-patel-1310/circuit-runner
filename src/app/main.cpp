// Circuit Runner — an arcade maze-chase game in modern C++20 + raylib.
//
//   circuit_runner                       play
//   circuit_runner --demo                watch the AI play
//   circuit_runner --replay FILE.crr     watch a recorded game
//   circuit_runner --seed 42             fixed seed (reproducible procedural levels)

#include <cstdio>
#include <filesystem>
#include <string>

#include "app/App.hpp"
#include "raylib.h"

namespace fs = std::filesystem;

namespace {

std::string findAssets(const std::string& override) {
  if (!override.empty()) return override;
  const std::string exeDir = GetApplicationDirectory();
  for (const std::string& candidate : {exeDir + "assets", std::string("assets"), std::string("../assets"), exeDir + "../assets"}) {
    std::error_code ec;
    if (fs::is_directory(candidate + "/levels", ec)) return candidate;
  }
  return "assets";
}

void usage() {
  std::puts(
      "Circuit Runner\n"
      "  --demo               start with the AI playing\n"
      "  --replay FILE        play back a recorded .crr replay\n"
      "  --seed N             fixed RNG seed\n"
      "  --assets DIR         asset directory (default: next to the executable)\n"
      "  --fullscreen         start in borderless fullscreen\n"
      "  --capture FILE N     save a screenshot at frame N and exit");
}

}  // namespace

int main(int argc, char** argv) {
  cr::app::AppConfig config;
  std::string assets;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool hasNext = i + 1 < argc;
    if (a == "--demo") {
      config.demo = true;
    } else if (a == "--fullscreen") {
      config.fullscreen = true;
    } else if (a == "--seed" && hasNext) {
      config.seed = std::stoull(argv[++i]);
      config.seedFixed = true;
    } else if (a == "--assets" && hasNext) {
      assets = argv[++i];
    } else if (a == "--replay" && hasNext) {
      std::string error;
      config.replay = cr::Replay::load(argv[++i], &error);
      if (!config.replay) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
      }
    } else if (a == "--capture" && i + 2 < argc) {
      config.capturePath = argv[++i];
      config.captureFrame = std::stoi(argv[++i]);
    } else {
      usage();
      return a == "--help" || a == "-h" ? 0 : 2;
    }
  }

  SetTraceLogLevel(LOG_WARNING);
  config.assetDir = findAssets(assets);
  config.dataDir = GetApplicationDirectory();
  if (config.dataDir.empty()) config.dataDir = ".";

  cr::app::App app(std::move(config));
  return app.run();
}
