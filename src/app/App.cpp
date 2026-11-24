#include "app/App.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "core/LevelLoader.hpp"

namespace cr::app {

namespace {

constexpr float kTickSeconds = 1.0f / kTickRate;
constexpr const char* kMenuItems[] = {"PLAY", "WATCH AI", "HIGH SCORES", "HOW TO PLAY", "QUIT"};
constexpr int kMenuCount = 5;

bool anyPressed(std::initializer_list<int> keys) {
  for (int k : keys)
    if (IsKeyPressed(k)) return true;
  return false;
}

bool anyDown(std::initializer_list<int> keys) {
  for (int k : keys)
    if (IsKeyDown(k)) return true;
  return false;
}

}  // namespace

App::App(AppConfig config) : config_(std::move(config)) {
  auto loaded = loadLevelsFromDirectory(config_.assetDir + "/levels");
  levels_ = std::move(loaded.levels);
  levelErrors_ = std::move(loaded.errors);
  for (const auto& e : levelErrors_) std::fprintf(stderr, "level warning: %s\n", e.c_str());
  scores_.load(config_.dataDir + "/highscores.txt");
}

int App::run() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
  InitWindow(kCanvasW, kCanvasH, "Circuit Runner");
  SetExitKey(KEY_NULL);  // Esc is used for pause/back
  // Fit the window to ~85% of the monitor height on first launch.
  const int monitorH = GetMonitorHeight(GetCurrentMonitor());
  if (monitorH > 0 && monitorH * 85 / 100 < kCanvasH) {
    const int h = monitorH * 85 / 100;
    SetWindowSize(kCanvasW * h / kCanvasH, h);
  }
  if (config_.fullscreen) ToggleBorderlessWindowed();
  SetTargetFPS(60);
  canvas_ = LoadRenderTexture(kCanvasW, kCanvasH);
  SetTextureFilter(canvas_.texture, TEXTURE_FILTER_BILINEAR);
  audio_.init();

  if (config_.replay) {
    // Replays always use their recorded seed and run with no input.
    playingBack_ = true;
    GameOptions o;
    o.seed = config_.replay->seed;
    o.startLives = config_.replay->startLives;
    o.levels = levels_;
    if (config_.replay->levelsHash != hashLevels(levels_))
      std::fprintf(stderr, "warning: replay was recorded with different levels; playback will diverge\n");
    game_ = std::make_unique<Game>(o);
    screen_ = Screen::Playing;
  } else if (config_.demo) {
    startGame(true);
  } else {
    startAttract();
  }

  while (!WindowShouldClose() && !quit_) {
    const float dt = std::min(GetFrameTime(), 0.25f);
    time_ += dt;
    update(dt);
    drawFrame();
    ++frame_;
    if (!config_.capturePath.empty() && frame_ >= config_.captureFrame) break;
  }

  UnloadRenderTexture(canvas_);
  audio_.shutdown();
  CloseWindow();
  return 0;
}

// ---------------------------------------------------------------- game flow

void App::startGame(bool demo) {
  GameOptions o;
  o.seed = config_.seedFixed ? config_.seed
                             : static_cast<std::uint64_t>(
                                   std::chrono::steady_clock::now().time_since_epoch().count());
  o.levels = levels_;
  game_ = std::make_unique<Game>(o);
  botControlled_ = demo;
  playingBack_ = false;
  recording_ = Replay{o.seed, o.startLives, hashLevels(levels_), {}};
  renderer_.clearEffects();
  accumulator_ = 0.0f;
  gameOverTimer_ = 0.0f;
  lastPressed_ = Dir::None;
  screen_ = Screen::Playing;
}

void App::startAttract() {
  startGame(true);
  screen_ = Screen::Title;
}

void App::finishGame() {
  if (!botControlled_ && !playingBack_) {
    recording_.save(config_.dataDir + "/last_replay.crr");
    if (scores_.qualifies(game_->score())) {
      nameEntry_ = "AAA";
      nameCursor_ = 0;
      screen_ = Screen::NameEntry;
      return;
    }
  }
  lastRank_ = -1;
  screen_ = Screen::HighScores;
  playingBack_ = false;
}

InputFrame App::readInput() {
  struct Binding {
    Dir dir;
    std::initializer_list<int> keys;
    int pad;
  };
  const Binding bindings[] = {
      {Dir::Up, {KEY_UP, KEY_W}, GAMEPAD_BUTTON_LEFT_FACE_UP},
      {Dir::Down, {KEY_DOWN, KEY_S}, GAMEPAD_BUTTON_LEFT_FACE_DOWN},
      {Dir::Left, {KEY_LEFT, KEY_A}, GAMEPAD_BUTTON_LEFT_FACE_LEFT},
      {Dir::Right, {KEY_RIGHT, KEY_D}, GAMEPAD_BUTTON_LEFT_FACE_RIGHT},
  };
  const bool pad = IsGamepadAvailable(0);
  for (const auto& b : bindings)
    if (anyPressed(b.keys) || (pad && IsGamepadButtonPressed(0, b.pad))) lastPressed_ = b.dir;

  InputFrame in;
  for (const auto& b : bindings) {
    if (b.dir == lastPressed_ && (anyDown(b.keys) || (pad && IsGamepadButtonDown(0, b.pad)))) in.dir = b.dir;
  }
  if (in.dir == Dir::None) {
    for (const auto& b : bindings)
      if (anyDown(b.keys) || (pad && IsGamepadButtonDown(0, b.pad))) in.dir = b.dir;
  }
  if (pad) {
    const float ax = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    const float ay = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
    if (std::max(std::abs(ax), std::abs(ay)) > 0.5f)
      in.dir = std::abs(ax) > std::abs(ay) ? (ax < 0 ? Dir::Left : Dir::Right) : (ay < 0 ? Dir::Up : Dir::Down);
  }
  if (IsKeyPressed(KEY_SPACE) || (pad && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)))
    overclockQueued_ = true;
  in.overclock = overclockQueued_;
  return in;
}

void App::simulate(float dt) {
  accumulator_ += dt;
  const InputFrame human = (botControlled_ || playingBack_) ? InputFrame{} : readInput();
  int steps = 0;
  while (accumulator_ >= kTickSeconds && steps < 8) {
    InputFrame in = human;
    if (botControlled_) {
      in = pilot_.decide(*game_);
    } else if (playingBack_) {
      in = playbackIndex_ < config_.replay->frames.size() ? config_.replay->frames[playbackIndex_++] : InputFrame{};
    } else {
      recording_.frames.push_back(in);
      overclockQueued_ = false;
    }
    game_->tick(in);
    handleEvents();
    accumulator_ -= kTickSeconds;
    ++steps;
  }
}

void App::handleEvents() {
  const bool quiet = screen_ == Screen::Title;  // attract mode is silent
  int blip = 0;
  for (const GameEvent& e : game_->drainEvents()) {
    renderer_.onEvent(e, *game_);
    if (quiet) continue;
    switch (e.type) {
      case EventType::PacketEaten: audio_.play((blip++ + game_->packetsEaten()) % 2 ? Sfx::Blip : Sfx::Blip2); break;
      case EventType::CoreEaten: audio_.play(Sfx::Core); break;
      case EventType::VirusQuarantined: audio_.play(Sfx::Quarantine); break;
      case EventType::PlayerCaught: audio_.play(Sfx::Caught); break;
      case EventType::LevelCleared: audio_.play(Sfx::LevelClear); break;
      case EventType::ExtraLife: audio_.play(Sfx::ExtraLife); break;
      case EventType::KeySpawned: audio_.play(Sfx::KeySpawn); break;
      case EventType::KeyCollected: audio_.play(Sfx::KeyGet); break;
      case EventType::GlitchBlink: audio_.play(Sfx::Blink); break;
      case EventType::OverclockStarted: audio_.play(Sfx::Overclock); break;
      case EventType::FreezeEnding: audio_.play(Sfx::FreezeEnding); break;
      default: break;
    }
  }
}

bool App::confirmPressed() const {
  return anyPressed({KEY_ENTER, KEY_SPACE, KEY_KP_ENTER}) ||
         (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
}

bool App::backPressed() const {
  return anyPressed({KEY_ESCAPE, KEY_BACKSPACE}) ||
         (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
}

void App::update(float dt) {
  renderer_.update(dt);

  // Global hotkeys.
  if (IsKeyPressed(KEY_M)) audio_.toggleMute();
  if (IsKeyPressed(KEY_F1)) showTargets_ = !showTargets_;
  if (IsKeyPressed(KEY_F11) || (IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_LEFT_ALT))) ToggleBorderlessWindowed();

  switch (screen_) {
    case Screen::Title: {
      simulate(dt);  // attract-mode game in the background
      if (game_->phase() == Phase::GameOver) startAttract();
      if (anyPressed({KEY_UP, KEY_W})) {
        menuIndex_ = (menuIndex_ + kMenuCount - 1) % kMenuCount;
        audio_.play(Sfx::MenuMove);
      }
      if (anyPressed({KEY_DOWN, KEY_S})) {
        menuIndex_ = (menuIndex_ + 1) % kMenuCount;
        audio_.play(Sfx::MenuMove);
      }
      if (confirmPressed()) {
        audio_.play(Sfx::MenuSelect);
        switch (menuIndex_) {
          case 0: startGame(false); break;
          case 1: startGame(true); break;
          case 2: lastRank_ = -1; screen_ = Screen::HighScores; break;
          case 3: screen_ = Screen::Help; break;
          case 4: quit_ = true; break;
        }
      }
      break;
    }
    case Screen::Playing: {
      if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE) ||
          (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) {
        if (botControlled_ || playingBack_) {
          startAttract();
        } else {
          screen_ = Screen::Paused;
        }
        break;
      }
      simulate(dt);
      if (game_->phase() == Phase::GameOver && game_->phaseTicks() <= 0) {
        // Linger on the game-over screen for a moment.
        gameOverTimer_ += dt;
        if (gameOverTimer_ > 2.5f) {
          gameOverTimer_ = 0.0f;
          finishGame();
        }
      }
      break;
    }
    case Screen::Paused:
      if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE) || confirmPressed()) screen_ = Screen::Playing;
      if (IsKeyPressed(KEY_Q)) startAttract();
      break;
    case Screen::NameEntry: {
      auto& c = nameEntry_[static_cast<std::size_t>(nameCursor_)];
      auto cycle = [](char ch, int step) {
        static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        const auto pos = alphabet.find(ch);
        const int n = static_cast<int>(alphabet.size());
        return alphabet[static_cast<std::size_t>((static_cast<int>(pos) + step + n) % n)];
      };
      if (anyPressed({KEY_UP, KEY_W})) c = cycle(c, 1);
      if (anyPressed({KEY_DOWN, KEY_S})) c = cycle(c, -1);
      if (anyPressed({KEY_RIGHT, KEY_D})) nameCursor_ = std::min(2, nameCursor_ + 1);
      if (anyPressed({KEY_LEFT, KEY_A})) nameCursor_ = std::max(0, nameCursor_ - 1);
      for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
        if (std::isalnum(ch)) {
          nameEntry_[static_cast<std::size_t>(nameCursor_)] = static_cast<char>(std::toupper(ch));
          nameCursor_ = std::min(2, nameCursor_ + 1);
        }
      }
      if (anyPressed({KEY_ENTER, KEY_KP_ENTER})) {
        lastRank_ = scores_.insert({nameEntry_, game_->score(), game_->level()});
        scores_.save(config_.dataDir + "/highscores.txt");
        audio_.play(Sfx::MenuSelect);
        screen_ = Screen::HighScores;
      }
      break;
    }
    case Screen::HighScores:
    case Screen::Help:
      if (confirmPressed() || backPressed()) startAttract();
      break;
  }
}

// ---------------------------------------------------------------- drawing

void App::drawFrame() {
  BeginTextureMode(canvas_);
  DrawOptions opts;
  opts.showTargets = showTargets_;
  opts.highScore = scores_.best();
  opts.dimmed = screen_ == Screen::Title || screen_ == Screen::HighScores || screen_ == Screen::Help ||
                screen_ == Screen::NameEntry;
  renderer_.draw(*game_, opts);

  switch (screen_) {
    case Screen::Title: drawTitle(); break;
    case Screen::Paused: drawPause(); break;
    case Screen::NameEntry: drawNameEntry(); break;
    case Screen::HighScores: drawHighScores(); break;
    case Screen::Help: drawHelp(); break;
    case Screen::Playing:
      if (botControlled_) Renderer::drawTextCentered("AI DEMO - ESC TO EXIT", kCanvasW / 2, kHudTop + 6, 14, palette::Warning);
      if (playingBack_) Renderer::drawTextCentered("REPLAY - ESC TO EXIT", kCanvasW / 2, kHudTop + 6, 14, palette::Warning);
      break;
  }
  if (audio_.muted()) DrawText("MUTED", 8, kCanvasH - kHudBottom - 18, 12, palette::Dim);
  if (showTargets_) DrawText("AI TARGETS [F1]", kCanvasW - 110, kCanvasH - kHudBottom - 18, 12, palette::Warning);
  EndTextureMode();

  // Letterbox the canvas into the window.
  BeginDrawing();
  ClearBackground(BLACK);
  const float scale = std::min(static_cast<float>(GetScreenWidth()) / kCanvasW,
                               static_cast<float>(GetScreenHeight()) / kCanvasH);
  const float w = kCanvasW * scale, h = kCanvasH * scale;
  const Rectangle src{0, 0, static_cast<float>(kCanvasW), -static_cast<float>(kCanvasH)};
  const Rectangle dst{(static_cast<float>(GetScreenWidth()) - w) / 2, (static_cast<float>(GetScreenHeight()) - h) / 2, w, h};
  DrawTexturePro(canvas_.texture, src, dst, {0, 0}, 0.0f, WHITE);
  EndDrawing();

  if (!config_.capturePath.empty() && frame_ + 1 >= config_.captureFrame) {
    Image img = LoadImageFromTexture(canvas_.texture);
    ImageFlipVertical(&img);
    ExportImage(img, config_.capturePath.c_str());
    UnloadImage(img);
  }
}

void App::drawTitle() {
  const int cx = kCanvasW / 2;
  // Logo: chip outline with the title inside.
  const Rectangle chip{static_cast<float>(cx - 230), 120, 460, 120};
  DrawRectangleRounded(chip, 0.12f, 8, {10, 24, 38, 240});
  DrawRectangleRoundedLinesEx(chip, 0.12f, 8, 3.0f, palette::Accent);
  for (int i = 0; i < 10; ++i) {
    const float x = chip.x + 30.0f + static_cast<float>(i) * 44.0f;
    DrawRectangleV({x, chip.y - 12}, {10, 12}, palette::Dim);
    DrawRectangleV({x, chip.y + chip.height}, {10, 12}, palette::Dim);
  }
  Renderer::drawTextCentered("CIRCUIT", cx, 138, 48, palette::Text);
  Renderer::drawTextCentered("RUNNER", cx, 186, 40, palette::Accent);

  Renderer::drawDrone({static_cast<float>(cx), 300}, 26, Dir::Right, time_);
  for (std::size_t i = 0; i < 4; ++i) {
    const auto k = kAllVirusKinds[i];
    const float x = static_cast<float>(cx - 150 + static_cast<int>(i) * 100);
    Renderer::drawVirusShape(k, {x, 370}, 18, time_, virusColor(k), Dir::Down);
    Renderer::drawTextCentered(virusName(k), static_cast<int>(x), 396, 12, virusColor(k));
  }

  for (int i = 0; i < kMenuCount; ++i) {
    const bool sel = i == menuIndex_;
    const int y = 450 + i * 40;
    if (sel) DrawRectangle(cx - 120, y - 6, 240, 32, {46, 230, 166, 40});
    Renderer::drawTextCentered(kMenuItems[i], cx, y, 22, sel ? palette::Accent : palette::Text);
  }
  if (!levelErrors_.empty())
    Renderer::drawTextCentered(TextFormat("%d level file(s) failed to load", static_cast<int>(levelErrors_.size())), cx,
                               kCanvasH - 90, 14, palette::Danger);
  Renderer::drawTextCentered("ARROWS / WASD  -  ENTER TO SELECT", cx, kCanvasH - 60, 14, palette::Dim);
}

void App::drawPause() {
  DrawRectangle(0, 0, kCanvasW, kCanvasH, {0, 0, 0, 160});
  Renderer::drawTextCentered("PAUSED", kCanvasW / 2, 320, 40, palette::Text);
  Renderer::drawTextCentered("P / ESC  resume", kCanvasW / 2, 380, 18, palette::Dim);
  Renderer::drawTextCentered("Q  quit to title", kCanvasW / 2, 406, 18, palette::Dim);
  Renderer::drawTextCentered("M  mute    F1  AI overlay    F11  fullscreen", kCanvasW / 2, 450, 14, palette::Dim);
}

void App::drawNameEntry() {
  const int cx = kCanvasW / 2;
  Renderer::drawTextCentered("NEW HIGH SCORE", cx, 220, 36, palette::Warning);
  Renderer::drawTextCentered(TextFormat("%d", game_->score()), cx, 270, 30, palette::Text);
  for (int i = 0; i < 3; ++i) {
    const int x = cx - 75 + i * 50;
    const bool sel = i == nameCursor_;
    DrawRectangleLines(x, 340, 44, 56, sel ? palette::Accent : palette::Dim);
    const char s[2] = {nameEntry_[static_cast<std::size_t>(i)], 0};
    Renderer::drawTextCentered(s, x + 22, 350, 40, sel ? palette::Accent : palette::Text);
  }
  Renderer::drawTextCentered("TYPE OR USE ARROWS - ENTER TO SAVE", cx, 430, 14, palette::Dim);
}

void App::drawHighScores() {
  const int cx = kCanvasW / 2;
  Renderer::drawTextCentered("HIGH SCORES", cx, 120, 36, palette::Accent);
  const auto& entries = scores_.entries();
  if (entries.empty()) Renderer::drawTextCentered("No scores yet - go set one!", cx, 220, 18, palette::Dim);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const int y = 190 + static_cast<int>(i) * 36;
    const Color c = static_cast<int>(i) == lastRank_ ? palette::Warning : palette::Text;
    DrawText(TextFormat("%2d.", static_cast<int>(i) + 1), cx - 180, y, 22, palette::Dim);
    DrawText(entries[i].name.c_str(), cx - 130, y, 22, c);
    const char* score = TextFormat("%d", entries[i].score);
    DrawText(score, cx + 70 - MeasureText(score, 22), y, 22, c);
    DrawText(TextFormat("LV %d", entries[i].level), cx + 100, y, 22, palette::Dim);
  }
  Renderer::drawTextCentered("ENTER TO CONTINUE", cx, kCanvasH - 80, 16, palette::Dim);
}

void App::drawHelp() {
  const int x = 60;
  int y = 100;
  auto line = [&](const char* text, Color c = palette::Text, int size = 18) {
    DrawText(text, x, y, size, c);
    y += size + 10;
  };
  Renderer::drawTextCentered("HOW TO PLAY", kCanvasW / 2, 50, 32, palette::Accent);
  line("Steer the drone with ARROWS / WASD / gamepad.");
  line("Collect every data packet to clean the sector.");
  line("Grab a firewall core to lock viruses down -");
  line("then ram them for 200 / 400 / 800 / 1600 points.");
  line("Packets charge OVERCLOCK. Press SPACE for a speed burst.");
  line("Bonus keys appear below the base. Grab them fast!");
  y += 10;
  line("THE VIRUSES", palette::Accent, 22);
  const char* desc[] = {"chases you along the shortest path", "predicts where you are heading",
                        "flanks you from the opposite side", "roams, then teleports nearby"};
  for (std::size_t i = 0; i < 4; ++i) {
    const auto k = kAllVirusKinds[i];
    Renderer::drawVirusShape(k, {static_cast<float>(x + 14), static_cast<float>(y + 10)}, 13, time_, virusColor(k), Dir::Right);
    DrawText(virusName(k), x + 40, y, 18, virusColor(k));
    DrawText(desc[i], x + 150, y, 18, palette::Text);
    y += 40;
  }
  y += 10;
  line("P/ESC pause   M mute   F1 AI overlay   F11 fullscreen", palette::Dim, 16);
  Renderer::drawTextCentered("ENTER TO GO BACK", kCanvasW / 2, kCanvasH - 60, 16, palette::Dim);
}

}  // namespace cr::app
