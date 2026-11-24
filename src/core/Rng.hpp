#pragma once

#include <cstdint>

namespace cr {

/// Small, fast, fully deterministic PRNG (SplitMix64).
/// std::mt19937 + distributions are not guaranteed to produce identical
/// sequences across standard library implementations, which would break
/// cross-platform replays, so we roll our own.
class Rng {
 public:
  explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull) : state_(seed) {}

  std::uint64_t next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  /// Uniform integer in [0, n).
  int uniform(int n) { return n <= 1 ? 0 : static_cast<int>(next() % static_cast<std::uint64_t>(n)); }

  /// Uniform integer in [lo, hi].
  int range(int lo, int hi) { return lo + uniform(hi - lo + 1); }

  bool chance(int percent) { return uniform(100) < percent; }

  std::uint64_t state() const { return state_; }

 private:
  std::uint64_t state_;
};

}  // namespace cr
