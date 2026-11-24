#include "core/Maze.hpp"

#include <algorithm>

namespace cr {

namespace {

std::string trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  const auto last = s.find_last_not_of(" \t");
  return std::string(s.substr(first, last - first + 1));
}

bool isPickup(Tile t) { return t == Tile::Packet || t == Tile::Core; }

}  // namespace

std::uint64_t fnv1a(std::string_view data, std::uint64_t seed) {
  std::uint64_t h = seed;
  for (unsigned char c : data) {
    h ^= c;
    h *= 0x100000001b3ull;
  }
  return h;
}

std::optional<Maze> Maze::parse(std::string_view text, std::string* error) {
  auto fail = [&](std::string msg) -> std::optional<Maze> {
    if (error) *error = std::move(msg);
    return std::nullopt;
  };

  Maze m;
  std::vector<std::string> rows;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string line(text.substr(pos, end - pos));
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() && line.front() == ';') {
      const auto key = line.find("name:");
      if (key != std::string::npos) m.name_ = trim(std::string_view(line).substr(key + 5));
      continue;
    }
    if (trim(line).empty()) continue;
    rows.push_back(std::move(line));
  }

  if (rows.size() < 5) return fail("maze needs at least 5 rows");
  std::size_t width = 0;
  for (const auto& r : rows) width = std::max(width, r.size());
  if (width < 5) return fail("maze needs at least 5 columns");
  // Editors often strip trailing whitespace; pad short rows with empty floor.
  for (auto& r : rows) r.resize(width, ' ');

  m.width_ = static_cast<int>(width);
  m.height_ = static_cast<int>(rows.size());
  m.tiles_.assign(static_cast<std::size_t>(m.cellCount()), Tile::Empty);
  m.base_.assign(static_cast<std::size_t>(m.cellCount()), 0);

  int players = 0, exits = 0, keys = 0, gates = 0;
  std::array<int, 3> slots{0, 0, 0};

  for (int y = 0; y < m.height_; ++y) {
    for (int x = 0; x < m.width_; ++x) {
      const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      const Vec2i p{x, y};
      const auto i = static_cast<std::size_t>(m.index(p));
      switch (c) {
        case '#': m.tiles_[i] = Tile::Wall; break;
        case '.': m.tiles_[i] = Tile::Packet; break;
        case 'o': m.tiles_[i] = Tile::Core; break;
        case '-': m.tiles_[i] = Tile::Gate; ++gates; break;
        case ' ':
        case '_': break;
        case 'B': m.base_[i] = 1; break;
        case '1':
        case '2':
        case '3': {
          const int s = c - '1';
          m.base_[i] = 1;
          m.spawns_.baseSlots[static_cast<std::size_t>(s)] = p;
          ++slots[static_cast<std::size_t>(s)];
          break;
        }
        case 'P': m.spawns_.player = p; ++players; break;
        case 'E': m.spawns_.baseExit = p; ++exits; break;
        case 'S': m.spawns_.keySpawn = p; ++keys; break;
        default:
          return fail("unexpected character '" + std::string(1, c) + "' at row " + std::to_string(y + 1) +
                      ", column " + std::to_string(x + 1));
      }
      if (isPickup(m.tiles_[i])) ++m.pickups_;
    }
  }

  if (players != 1) return fail("maze must contain exactly one 'P' (player spawn)");
  if (exits != 1) return fail("maze must contain exactly one 'E' (base exit)");
  if (keys != 1) return fail("maze must contain exactly one 'S' (key spawn)");
  if (gates < 1) return fail("maze must contain at least one '-' (base gate)");
  for (int s = 0; s < 3; ++s) {
    if (slots[static_cast<std::size_t>(s)] != 1)
      return fail("maze must contain exactly one '" + std::to_string(s + 1) + "' (virus slot)");
  }
  if (m.pickups_ == 0) return fail("maze has no packets to collect");

  m.totalPickups_ = m.pickups_;
  return m;
}

void Maze::set(Vec2i p, Tile t) {
  const auto i = static_cast<std::size_t>(index(wrap(p)));
  if (isPickup(tiles_[i])) --pickups_;
  if (isPickup(t)) ++pickups_;
  tiles_[i] = t;
}

std::string Maze::serialize() const {
  std::string out = "; name: " + name_ + "\n";
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const Vec2i p{x, y};
      char c = ' ';
      switch (at(p)) {
        case Tile::Wall: c = '#'; break;
        case Tile::Packet: c = '.'; break;
        case Tile::Core: c = 'o'; break;
        case Tile::Gate: c = '-'; break;
        case Tile::Empty: c = isBase(p) ? 'B' : ' '; break;
      }
      for (std::size_t s = 0; s < 3; ++s)
        if (spawns_.baseSlots[s] == p) c = static_cast<char>('1' + s);
      if (p == spawns_.player) c = 'P';
      if (p == spawns_.baseExit) c = 'E';
      if (p == spawns_.keySpawn) c = 'S';
      out += c;
    }
    out += '\n';
  }
  return out;
}

std::uint64_t Maze::contentHash() const { return fnv1a(serialize()); }

}  // namespace cr
