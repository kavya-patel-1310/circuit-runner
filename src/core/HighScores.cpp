#include "core/HighScores.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace cr {

std::string HighScoreTable::sanitizeName(std::string_view name) {
  std::string out;
  for (char c : name) {
    const auto u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) out += static_cast<char>(std::toupper(u));
    if (out.size() == 3) break;
  }
  while (out.size() < 3) out += 'A';
  return out;
}

bool HighScoreTable::qualifies(int score) const {
  if (score <= 0) return false;
  return entries_.size() < kMaxEntries || score > entries_.back().score;
}

int HighScoreTable::insert(ScoreEntry entry) {
  if (!qualifies(entry.score)) return -1;
  entry.name = sanitizeName(entry.name);
  // Ties keep the older entry ahead (upper_bound).
  const auto pos = std::upper_bound(entries_.begin(), entries_.end(), entry.score,
                                    [](int s, const ScoreEntry& e) { return s > e.score; });
  const auto rank = static_cast<int>(pos - entries_.begin());
  entries_.insert(pos, std::move(entry));
  if (entries_.size() > kMaxEntries) entries_.resize(kMaxEntries);
  return rank;
}

std::string HighScoreTable::toString() const {
  std::ostringstream out;
  for (const auto& e : entries_) out << e.name << ' ' << e.score << ' ' << e.level << '\n';
  return out.str();
}

HighScoreTable HighScoreTable::fromString(std::string_view text) {
  HighScoreTable table;
  std::istringstream in{std::string(text)};
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    ScoreEntry e;
    if (ls >> e.name >> e.score >> e.level) table.insert(e);
  }
  return table;
}

bool HighScoreTable::load(const std::string& path) {
  std::ifstream in(path);
  if (!in) return false;
  std::stringstream buffer;
  buffer << in.rdbuf();
  *this = fromString(buffer.str());
  return true;
}

bool HighScoreTable::save(const std::string& path) const {
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  out << toString();
  return static_cast<bool>(out);
}

}  // namespace cr
