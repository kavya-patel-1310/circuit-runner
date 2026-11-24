#pragma once

#include <array>
#include <cstdint>

#include "core/Mover.hpp"

namespace cr {

/// Four enemies, each with its own hunting strategy:
///  - Worm:    direct pursuit along the shortest path to the player.
///  - Trojan:  predicts where the player will be a few tiles ahead (reads input).
///  - Rootkit: pincer — targets the junction near the player that is farthest
///             from the Worm, so the two attack from opposite sides.
///  - Glitch:  alternates between chasing and roaming, and periodically
///             teleports ("blinks") to a tile a medium distance from the player.
enum class VirusKind : std::uint8_t { Worm, Trojan, Rootkit, Glitch };

inline constexpr std::array<VirusKind, 4> kAllVirusKinds{VirusKind::Worm, VirusKind::Trojan, VirusKind::Rootkit,
                                                         VirusKind::Glitch};

constexpr const char* virusName(VirusKind k) {
  switch (k) {
    case VirusKind::Worm: return "WORM";
    case VirusKind::Trojan: return "TROJAN";
    case VirusKind::Rootkit: return "ROOTKIT";
    case VirusKind::Glitch: return "GLITCH";
  }
  return "?";
}

enum class VirusState : std::uint8_t {
  InBase,       // waiting for release
  Leaving,      // pathing out through the gate
  Active,       // patrolling or hunting (dangerous)
  Frozen,       // firewall active: slow, random, can be quarantined
  Quarantined,  // caught; racing back to base to respawn
};

struct Virus {
  VirusKind kind = VirusKind::Worm;
  VirusState state = VirusState::InBase;
  Mover mover;
  Vec2i homeSlot;      // where it respawns inside the base
  Vec2i patrolCorner;  // its home corner during Patrol mode
  Vec2i target;        // last computed target (exposed for the debug overlay)
  int releaseCountdown = 0;
  bool reversePending = false;
  // Glitch-specific behaviour
  int blinkCountdown = 0;
  int retargetCountdown = 0;
  bool chasing = true;
  Vec2i roamTarget;

  bool dangerous() const { return state == VirusState::Active; }
  bool catchable() const { return state == VirusState::Frozen; }
};

}  // namespace cr
