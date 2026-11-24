#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/Audio.hpp"
#include "app/Renderer.hpp"
#include "core/AutoPilot.hpp"
#include "core/Game.hpp"
#include "core/HighScores.hpp"
#include "core/Replay.hpp"

namespace cr::app {

struct AppConfig {
  std::string assetDir;
  std::string dataDir;  // high scores + last replay
  std::uint64_t seed = 0;
  bool seedFixed = false;
  std::optional<Replay> replay;  // play this back instead of taking input
  bool demo = false;             // start with the bot playing
  bool fullscreen = false;
  std::string capturePath;       // screenshot then exit (docs / CI)
  int captureFrame = 0;
};

/// Screen state machine + fixed-timestep loop around the core simulation.
class App {
 public:
  explicit App(AppConfig config);
  int run();

 private:
  enum class Screen { Title, Playing, Paused, NameEntry, HighScores, Help };

  void startGame(bool demo);
  void startAttract();
  void finishGame();
  InputFrame readInput();
  void simulate(float dt);
  void update(float dt);
  void drawFrame();
  void drawTitle();
  void drawPause();
  void drawNameEntry();
  void drawHighScores();
  void drawHelp();
  void handleEvents();
  bool confirmPressed() const;
  bool backPressed() const;

  AppConfig config_;
  std::vector<Maze> levels_;
  std::vector<std::string> levelErrors_;
  HighScoreTable scores_;
  AudioSystem audio_;
  Renderer renderer_;
  AutoPilot pilot_;

  Screen screen_ = Screen::Title;
  std::unique_ptr<Game> game_;
  bool botControlled_ = false;
  bool playingBack_ = false;
  std::size_t playbackIndex_ = 0;
  Replay recording_;
  bool showTargets_ = false;
  float accumulator_ = 0.0f;
  float gameOverTimer_ = 0.0f;
  float time_ = 0.0f;
  int frame_ = 0;
  bool quit_ = false;

  int menuIndex_ = 0;
  std::string nameEntry_ = "AAA";
  int nameCursor_ = 0;
  int lastRank_ = -1;
  Dir lastPressed_ = Dir::None;
  bool overclockQueued_ = false;
  RenderTexture2D canvas_{};
};

}  // namespace cr::app
