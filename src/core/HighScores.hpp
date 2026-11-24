#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cr {

struct ScoreEntry {
  std::string name;  // 3 characters, A-Z / 0-9
  int score = 0;
  int level = 1;
};

/// Persistent top-10 table stored as a small text file.
class HighScoreTable {
 public:
  static constexpr std::size_t kMaxEntries = 10;

  bool qualifies(int score) const;
  /// Inserts in sorted position; returns the 0-based rank or -1 if it didn't place.
  int insert(ScoreEntry entry);
  const std::vector<ScoreEntry>& entries() const { return entries_; }
  int best() const { return entries_.empty() ? 0 : entries_.front().score; }

  std::string toString() const;
  static HighScoreTable fromString(std::string_view text);
  bool load(const std::string& path);
  bool save(const std::string& path) const;

  static std::string sanitizeName(std::string_view name);

 private:
  std::vector<ScoreEntry> entries_;
};

}  // namespace cr
