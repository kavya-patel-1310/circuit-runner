#pragma once

#include <array>
#include <cstddef>

#include "raylib.h"

namespace cr::app {

enum class Sfx : std::size_t {
  Blip,
  Blip2,
  Core,
  Quarantine,
  Caught,
  LevelClear,
  ExtraLife,
  KeySpawn,
  KeyGet,
  Blink,
  Overclock,
  FreezeEnding,
  MenuMove,
  MenuSelect,
  Count
};

/// Every sound effect is synthesised at start-up from simple waveforms, so
/// the game ships with zero audio asset files.
class AudioSystem {
 public:
  bool init();
  void shutdown();
  void play(Sfx sfx);
  void toggleMute() { muted_ = !muted_; }
  bool muted() const { return muted_; }
  bool ready() const { return ready_; }

 private:
  std::array<Sound, static_cast<std::size_t>(Sfx::Count)> sounds_{};
  bool ready_ = false;
  bool muted_ = false;
};

}  // namespace cr::app
