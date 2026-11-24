#pragma once

#include <cstdint>

#include "core/Types.hpp"

namespace cr {

/// The simulation never talks to audio or graphics directly. It emits events
/// and the front-end decides how to present them (sound, particles, text).
enum class EventType : std::uint8_t {
  LevelStarted,
  PacketEaten,
  CoreEaten,
  VirusQuarantined,
  PlayerCaught,
  LifeLost,
  GameOver,
  LevelCleared,
  ExtraLife,
  KeySpawned,
  KeyCollected,
  KeyExpired,
  GlitchTelegraph,
  GlitchBlink,
  OverclockStarted,
  FreezeEnding,
  FreezeEnded,
  ModeChanged,
};

struct GameEvent {
  EventType type;
  Vec2i tile{};
  Vec2i tile2{};
  int value = 0;
};

}  // namespace cr
