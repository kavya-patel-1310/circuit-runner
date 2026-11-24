#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Types.hpp"

namespace cr {

enum class Tile : std::uint8_t { Empty, Wall, Packet, Core, Gate };

struct MazeSpawns {
  Vec2i player;
  Vec2i baseExit;  // tile just outside the gate
  Vec2i keySpawn;  // where bonus keys appear
  std::array<Vec2i, 3> baseSlots;
};

/// Tile grid loaded from a plain-text level file.
///
/// Legend:
///   #  wall           .  data packet      o  firewall core (power-up)
///   -  base gate      B  base interior    1 2 3  virus start slots (in base)
///   P  player spawn   E  base exit        S  key spawn
///   (space) or _      empty floor
///   Lines starting with ';' are metadata, e.g. "; name: Motherboard".
///
/// Edges wrap around, so an open tile on the border forms a tunnel.
class Maze {
 public:
  static std::optional<Maze> parse(std::string_view text, std::string* error = nullptr);

  int width() const { return width_; }
  int height() const { return height_; }
  int cellCount() const { return width_ * height_; }
  const std::string& name() const { return name_; }
  const MazeSpawns& spawns() const { return spawns_; }

  Vec2i wrap(Vec2i p) const { return {floorMod(p.x, width_), floorMod(p.y, height_)}; }
  int index(Vec2i wrapped) const { return wrapped.y * width_ + wrapped.x; }

  Tile at(Vec2i p) const { return tiles_[static_cast<std::size_t>(index(wrap(p)))]; }
  void set(Vec2i p, Tile t);

  bool isWall(Vec2i p) const { return at(p) == Tile::Wall; }
  bool isBase(Vec2i p) const { return base_[static_cast<std::size_t>(index(wrap(p)))] != 0; }

  /// Walkable for the player and for viruses roaming the maze.
  bool isOpen(Vec2i p) const {
    const Tile t = at(p);
    return t != Tile::Wall && t != Tile::Gate && !isBase(p);
  }
  /// Walkable for viruses entering or leaving their base (gate is passable).
  bool isOpenForBaseTraffic(Vec2i p) const { return at(p) != Tile::Wall; }

  int pickupsRemaining() const { return pickups_; }
  int totalPickups() const { return totalPickups_; }

  std::string serialize() const;
  std::uint64_t contentHash() const;

 private:
  int width_ = 0;
  int height_ = 0;
  std::vector<Tile> tiles_;
  std::vector<std::uint8_t> base_;
  MazeSpawns spawns_{};
  std::string name_ = "Untitled";
  int pickups_ = 0;
  int totalPickups_ = 0;
};

/// FNV-1a, used for state hashes and level fingerprints.
std::uint64_t fnv1a(std::string_view data, std::uint64_t seed = 0xcbf29ce484222325ull);

}  // namespace cr
