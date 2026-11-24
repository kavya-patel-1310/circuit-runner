#include <gtest/gtest.h>

#include "core/HighScores.hpp"

using namespace cr;

TEST(HighScores, KeepsEntriesSortedDescending) {
  HighScoreTable t;
  t.insert({"AAA", 100, 1});
  t.insert({"BBB", 300, 2});
  t.insert({"CCC", 200, 1});
  ASSERT_EQ(t.entries().size(), 3u);
  EXPECT_EQ(t.entries()[0].name, "BBB");
  EXPECT_EQ(t.entries()[2].name, "AAA");
  EXPECT_EQ(t.best(), 300);
}

TEST(HighScores, CapsAtTenAndRejectsLowScores) {
  HighScoreTable t;
  for (int i = 1; i <= 12; ++i) t.insert({"XYZ", i * 10, 1});
  EXPECT_EQ(t.entries().size(), HighScoreTable::kMaxEntries);
  EXPECT_EQ(t.entries().back().score, 30);
  EXPECT_FALSE(t.qualifies(30));
  EXPECT_TRUE(t.qualifies(31));
  EXPECT_EQ(t.insert({"LOW", 5, 1}), -1);
}

TEST(HighScores, TiesKeepTheOlderEntryFirst) {
  HighScoreTable t;
  t.insert({"OLD", 500, 1});
  EXPECT_EQ(t.insert({"NEW", 500, 1}), 1);
}

TEST(HighScores, SanitizesNames) {
  EXPECT_EQ(HighScoreTable::sanitizeName("ab"), "ABA");
  EXPECT_EQ(HighScoreTable::sanitizeName("j-o-e!!"), "JOE");
  EXPECT_EQ(HighScoreTable::sanitizeName(""), "AAA");
}

TEST(HighScores, RoundTripsThroughText) {
  HighScoreTable t;
  t.insert({"ACE", 12345, 4});
  t.insert({"BOB", 999, 1});
  const HighScoreTable u = HighScoreTable::fromString(t.toString());
  ASSERT_EQ(u.entries().size(), 2u);
  EXPECT_EQ(u.entries()[0].name, "ACE");
  EXPECT_EQ(u.entries()[0].score, 12345);
  EXPECT_EQ(u.entries()[0].level, 4);
}

TEST(HighScores, IgnoresGarbageLines) {
  const HighScoreTable t = HighScoreTable::fromString("ACE 100 2\nnot a score\n\nBOB 50 1\n");
  EXPECT_EQ(t.entries().size(), 2u);
}
