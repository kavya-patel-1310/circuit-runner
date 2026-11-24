#include "core/MazeGenerator.hpp"

#include <array>
#include <stdexcept>
#include <vector>

#include "core/Rng.hpp"

namespace cr {

namespace {

constexpr int W = MazeGenerator::kWidth;
constexpr int H = MazeGenerator::kHeight;
constexpr int kCenterX = W / 2;  // 13
// Cells live at odd tile coordinates. The left half (including the centre
// column) has 7 x 14 cells.
constexpr int kCellsX = kCenterX / 2 + 1;  // 7
constexpr int kCellsY = (H - 1) / 2;       // 14

using Grid = std::vector<std::string>;

bool inBaseBlock(int x, int y) { return x >= 10 && x <= 16 && y >= 12 && y <= 16; }

bool isBaseChar(char c) { return c == 'B' || c == '1' || c == '2' || c == '3'; }

bool walkable(const Grid& g, int x, int y) {
  const char c = g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
  return c != '#' && c != '-' && !isBaseChar(c);
}

void setMirrored(Grid& g, int x, int y, char c) {
  g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = c;
  g[static_cast<std::size_t>(y)][static_cast<std::size_t>(W - 1 - x)] = c;
}

int openNeighbourCount(const Grid& g, int cx, int cy) {
  const int tx = cx * 2 + 1, ty = cy * 2 + 1;
  int n = 0;
  for (Dir d : kAllDirs) {
    const Vec2i v = delta(d);
    const int wx = tx + v.x, wy = ty + v.y;
    if (wx <= 0 || wy <= 0 || wx >= W - 1 || wy >= H - 1) continue;
    if (g[static_cast<std::size_t>(wy)][static_cast<std::size_t>(wx)] != '#') ++n;
  }
  return n;
}

void carveHalf(Grid& g, Rng& rng) {
  std::vector<std::uint8_t> visited(kCellsX * kCellsY, 0);
  auto cellIndex = [](int cx, int cy) { return static_cast<std::size_t>(cy * kCellsX + cx); };

  std::vector<Vec2i> stack;
  const Vec2i start{kCellsX - 1, 10};  // under the base, where the player spawns
  stack.push_back(start);
  visited[cellIndex(start.x, start.y)] = 1;
  g[static_cast<std::size_t>(start.y * 2 + 1)][static_cast<std::size_t>(start.x * 2 + 1)] = '.';

  while (!stack.empty()) {
    const Vec2i cur = stack.back();
    std::array<Vec2i, 4> options{};
    int count = 0;
    for (Dir d : kAllDirs) {
      const Vec2i n = cur + delta(d);
      if (n.x < 0 || n.y < 0 || n.x >= kCellsX || n.y >= kCellsY) continue;
      if (visited[cellIndex(n.x, n.y)]) continue;
      options[static_cast<std::size_t>(count++)] = n;
    }
    if (count == 0) {
      stack.pop_back();
      continue;
    }
    const Vec2i next = options[static_cast<std::size_t>(rng.uniform(count))];
    visited[cellIndex(next.x, next.y)] = 1;
    const int wx = cur.x + next.x + 1;  // midpoint in tile space
    const int wy = cur.y + next.y + 1;
    g[static_cast<std::size_t>(wy)][static_cast<std::size_t>(wx)] = '.';
    g[static_cast<std::size_t>(next.y * 2 + 1)][static_cast<std::size_t>(next.x * 2 + 1)] = '.';
    stack.push_back(next);
  }
}

void braidAndLoop(Grid& g, Rng& rng) {
  // Remove dead ends.
  for (int cy = 0; cy < kCellsY; ++cy) {
    for (int cx = 0; cx < kCellsX; ++cx) {
      if (openNeighbourCount(g, cx, cy) != 1) continue;
      std::array<Vec2i, 4> walls{};
      int count = 0;
      for (Dir d : kAllDirs) {
        const Vec2i v = delta(d);
        const int ncx = cx + v.x, ncy = cy + v.y;
        if (ncx < 0 || ncy < 0 || ncx >= kCellsX || ncy >= kCellsY) continue;
        const int wx = cx * 2 + 1 + v.x, wy = cy * 2 + 1 + v.y;
        if (g[static_cast<std::size_t>(wy)][static_cast<std::size_t>(wx)] == '#')
          walls[static_cast<std::size_t>(count++)] = {wx, wy};
      }
      if (count > 0) {
        const Vec2i w = walls[static_cast<std::size_t>(rng.uniform(count))];
        g[static_cast<std::size_t>(w.y)][static_cast<std::size_t>(w.x)] = '.';
      }
    }
  }
  // A few extra loops for more interesting chases.
  for (int y = 1; y < H - 1; ++y) {
    for (int x = 1; x <= kCenterX; ++x) {
      const bool horizontalWall = (x % 2 == 0) && (y % 2 == 1);
      const bool verticalWall = (x % 2 == 1) && (y % 2 == 0);
      if (!horizontalWall && !verticalWall) continue;
      if (g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#' && rng.chance(8))
        g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = '.';
    }
  }
}

void stampFeatures(Grid& g) {
  // Corridor ring around the base, then the base itself.
  for (int y = 11; y <= 17; ++y)
    for (int x = 9; x <= 17; ++x) g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = ' ';
  for (int y = 12; y <= 16; ++y)
    for (int x = 10; x <= 16; ++x) g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = '#';
  for (int y = 13; y <= 15; ++y)
    for (int x = 11; x <= 15; ++x) g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = 'B';
  g[14][12] = '1';
  g[14][13] = '2';
  g[14][14] = '3';
  g[12][13] = '-';
  g[11][13] = 'E';
  g[17][13] = 'S';
  // Side tunnel.
  for (int x : {0, 1, W - 2, W - 1}) g[13][static_cast<std::size_t>(x)] = ' ';
  g[21][13] = 'P';
  for (Vec2i c : {Vec2i{1, 3}, Vec2i{W - 2, 3}, Vec2i{1, H - 4}, Vec2i{W - 2, H - 4}})
    g[static_cast<std::size_t>(c.y)][static_cast<std::size_t>(c.x)] = 'o';
}

void repairConnectivity(Grid& g, Rng& rng) {
  for (int iteration = 0; iteration < 256; ++iteration) {
    std::vector<std::uint8_t> seen(W * H, 0);
    std::vector<Vec2i> queue{{kCenterX, 21}};
    seen[static_cast<std::size_t>(21 * W + kCenterX)] = 1;
    for (std::size_t head = 0; head < queue.size(); ++head) {
      const Vec2i p = queue[head];
      for (Dir d : kAllDirs) {
        Vec2i n = p + delta(d);
        n.x = floorMod(n.x, W);
        if (n.y < 0 || n.y >= H) continue;
        const auto i = static_cast<std::size_t>(n.y * W + n.x);
        if (seen[i] || !walkable(g, n.x, n.y)) continue;
        seen[i] = 1;
        queue.push_back(n);
      }
    }

    std::vector<Vec2i> candidates;
    bool allReached = true;
    for (int y = 1; y < H - 1; ++y) {
      for (int x = 1; x < W - 1; ++x) {
        if (walkable(g, x, y) && !seen[static_cast<std::size_t>(y * W + x)]) allReached = false;
        if (g[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] != '#' || inBaseBlock(x, y)) continue;
        auto reached = [&](int ax, int ay) { return walkable(g, ax, ay) && seen[static_cast<std::size_t>(ay * W + ax)]; };
        auto lost = [&](int ax, int ay) { return walkable(g, ax, ay) && !seen[static_cast<std::size_t>(ay * W + ax)]; };
        if ((reached(x - 1, y) && lost(x + 1, y)) || (lost(x - 1, y) && reached(x + 1, y)) ||
            (reached(x, y - 1) && lost(x, y + 1)) || (lost(x, y - 1) && reached(x, y + 1)))
          candidates.push_back({x, y});
      }
    }
    if (allReached) return;
    if (candidates.empty()) throw std::runtime_error("maze generator could not connect all regions");
    const Vec2i w = candidates[static_cast<std::size_t>(rng.uniform(static_cast<int>(candidates.size())))];
    setMirrored(g, w.x, w.y, '.');
  }
  throw std::runtime_error("maze generator exceeded repair budget");
}

}  // namespace

std::string MazeGenerator::generateText(std::uint64_t seed) {
  Rng rng(seed);
  Grid g(H, std::string(W, '#'));
  carveHalf(g, rng);
  braidAndLoop(g, rng);
  for (auto& row : g)
    for (int x = 0; x < kCenterX; ++x) row[static_cast<std::size_t>(W - 1 - x)] = row[static_cast<std::size_t>(x)];
  stampFeatures(g);
  repairConnectivity(g, rng);

  std::string text = "; name: Sector " + std::to_string(seed % 1000) + "\n";
  for (const auto& row : g) text += row + "\n";
  return text;
}

Maze MazeGenerator::generate(std::uint64_t seed) {
  std::string error;
  auto maze = Maze::parse(generateText(seed), &error);
  if (!maze) throw std::runtime_error("generated maze failed validation: " + error);
  return *maze;
}

}  // namespace cr
