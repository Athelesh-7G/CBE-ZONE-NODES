// Deterministic randomness. std::mt19937_64's raw output is fully specified by the
// standard, but std::uniform_int_distribution is NOT (libstdc++ and libc++ differ),
// so all derived values here are computed by hand. The same seed therefore gives
// the same graphs on every compiler and platform.
#pragma once

#include <cstdint>
#include <random>

namespace cbe {

inline std::uint64_t splitmix64(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// Per-case seed derived from a master seed, so one failing case can be replayed
// alone without regenerating the cases before it.
inline std::uint64_t deriveSeed(std::uint64_t master, std::uint64_t index) {
  return splitmix64(master ^ splitmix64(index + 1));
}

class Rng {
 public:
  explicit Rng(std::uint64_t seed) : eng_(seed) {}

  std::uint64_t next() { return eng_(); }

  // Uniform in [0, n). Modulo bias is below n / 2^64, irrelevant here.
  std::uint64_t below(std::uint64_t n) { return n == 0 ? 0 : next() % n; }

  // Uniform in [lo, hi] inclusive.
  std::int64_t range(std::int64_t lo, std::int64_t hi) {
    return lo + static_cast<std::int64_t>(below(static_cast<std::uint64_t>(hi - lo) + 1));
  }

  // Uniform in [0, 1).
  double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }

  bool chance(double p) { return unit() < p; }

 private:
  std::mt19937_64 eng_;
};

}  // namespace cbe
