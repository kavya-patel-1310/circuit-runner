#include "core/Replay.hpp"

#include <fstream>
#include <sstream>

namespace cr {

namespace {
constexpr const char* kMagic = "CIRCUITRUNNER-REPLAY";
constexpr int kVersion = 1;
}  // namespace

std::uint64_t hashLevels(const std::vector<Maze>& levels) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  for (const Maze& m : levels) h = fnv1a(m.serialize(), h);
  return h;
}

std::string Replay::toString() const {
  std::ostringstream out;
  out << kMagic << ' ' << kVersion << '\n';
  out << "seed " << seed << '\n';
  out << "lives " << startLives << '\n';
  out << "levels " << std::hex << levelsHash << std::dec << '\n';

  std::vector<std::pair<int, InputFrame>> runs;
  for (const InputFrame& f : frames) {
    if (!runs.empty() && runs.back().second == f) ++runs.back().first;
    else runs.emplace_back(1, f);
  }
  out << "runs " << runs.size() << '\n';
  for (const auto& [count, f] : runs)
    out << count << ' ' << static_cast<int>(f.dir) << ' ' << (f.overclock ? 1 : 0) << '\n';
  return out.str();
}

std::optional<Replay> Replay::fromString(std::string_view text, std::string* error) {
  auto fail = [&](const char* msg) -> std::optional<Replay> {
    if (error) *error = msg;
    return std::nullopt;
  };
  std::istringstream in{std::string(text)};
  std::string magic, key;
  int version = 0;
  if (!(in >> magic >> version) || magic != kMagic) return fail("not a Circuit Runner replay file");
  if (version != kVersion) return fail("unsupported replay version");

  Replay r;
  std::size_t runCount = 0;
  if (!(in >> key >> r.seed) || key != "seed") return fail("missing seed");
  if (!(in >> key >> r.startLives) || key != "lives") return fail("missing lives");
  if (!(in >> key >> std::hex >> r.levelsHash >> std::dec) || key != "levels") return fail("missing level hash");
  if (!(in >> key >> runCount) || key != "runs") return fail("missing run count");

  for (std::size_t i = 0; i < runCount; ++i) {
    int count = 0, dir = 0, overclock = 0;
    if (!(in >> count >> dir >> overclock) || count <= 0 || dir < 0 || dir > 4) return fail("corrupt input run");
    const InputFrame f{static_cast<Dir>(dir), overclock != 0};
    r.frames.insert(r.frames.end(), static_cast<std::size_t>(count), f);
  }
  return r;
}

bool Replay::save(const std::string& path) const {
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  out << toString();
  return static_cast<bool>(out);
}

std::optional<Replay> Replay::load(const std::string& path, std::string* error) {
  std::ifstream in(path);
  if (!in) {
    if (error) *error = "cannot open " + path;
    return std::nullopt;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  return fromString(buffer.str(), error);
}

ReplayResult runReplay(const Replay& replay, const std::vector<Maze>& levels) {
  ReplayResult result;
  if (replay.levelsHash != hashLevels(levels)) {
    result.error = "replay was recorded with a different level set";
    return result;
  }
  GameOptions options;
  options.seed = replay.seed;
  options.startLives = replay.startLives;
  options.levels = levels;
  Game game(options);
  for (const InputFrame& f : replay.frames) game.tick(f);
  result.ok = true;
  result.score = game.score();
  result.level = game.level();
  result.ticks = game.tickCount();
  result.finalHash = game.stateHash();
  return result;
}

}  // namespace cr
