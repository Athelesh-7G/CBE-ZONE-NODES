// Helpers for seeded, repeatable property tests.
//
// Every case gets its own Rng seeded from (master seed, test name, case index), so
//   cbe_tests --filter Properties.Foo --seed S --case K
// regenerates exactly case K of that test, and the failure message prints that line.
#pragma once

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "cbe/differential.hpp"
#include "cbe/generator.hpp"
#include "cbe/rng.hpp"
#include "cbe/solvers.hpp"
#include "cbe/uut.hpp"
#include "tt/tinytest.hpp"

namespace cbe_test {

using cbe::Graph;
using cbe::Result;
using cbe::Weight;

struct Ctx {
  std::string test;
  std::uint64_t seed = 0;
  std::size_t index = 0;
  std::string label;
  bool failed = false;

  std::string repro() const {
    const std::string mutant = cbe::activeMutant() == cbe::Mutant::None
                                   ? ""
                                   : std::string(" --mutant ") + cbe::mutantName(cbe::activeMutant());
    return "[repro: --filter " + test + " --seed " + std::to_string(seed) + " --case " +
           std::to_string(index) + mutant + " ; " + label + "] ";
  }
};

inline std::uint64_t nameHash(const std::string& s) {
  std::uint64_t h = 1469598103934665603ULL;  // FNV-1a
  for (unsigned char c : s) h = (h ^ c) * 1099511628211ULL;
  return h;
}

inline long long numericOption(const char* key, long long fallback) {
  const std::string v = tt::option(key);
  return v.empty() ? fallback : std::atoll(v.c_str());
}

// Total random cases executed in this process, for reporting.
inline std::size_t& casesExecuted() {
  static std::size_t n = 0;
  return n;
}

// Runs `body(ctx, rng, graph)` over seeded generated graphs. Stops after three
// failing cases to keep the output readable.
template <class Body>
std::size_t forEachCase(const std::string& test, std::size_t defaultCases, Body&& body) {
  const std::size_t cases = static_cast<std::size_t>(numericOption("cases", static_cast<long long>(defaultCases)));
  const int maxN = static_cast<int>(numericOption("max-n", 200));
  const long long only = numericOption("case", -1);
  const std::uint64_t seedForTest = cbe::splitmix64(tt::seed() ^ nameHash(test));
  std::size_t ran = 0;
  int failures = 0;
  for (std::size_t i = 0; i < cases && failures < 3; ++i) {
    if (only >= 0 && static_cast<long long>(i) != only) continue;
    cbe::Rng rng(cbe::deriveSeed(seedForTest, i));
    cbe::GeneratedCase gc = cbe::generateCase(rng, maxN);
    Ctx ctx{test, tt::seed(), i, gc.label, false};
    body(ctx, rng, gc.graph);
    ++ran;
    ++casesExecuted();
    if (ctx.failed) ++failures;
  }
  std::cout << "    " << test << ": " << ran << " random cases (seed " << tt::seed() << ")\n";
  return ran;
}

// Use inside a property body. Records a failure with the repro line.
#define PROP_CHECK(ctx, cond, msg)                                       \
  do {                                                                   \
    if (!(cond)) {                                                       \
      const bool first_ = !(ctx).failed; /* report one failure per case */ \
      (ctx).failed = true;                                               \
      if (first_) TT_EXPECT_MSG(false, (ctx).repro() << #cond << " | " << msg); \
    }                                                                    \
  } while (0)

// All sources for small graphs, a random sample for large ones.
inline std::vector<int> pickSources(cbe::Rng& rng, const Graph& g, int maxAll = 30, int sample = 6) {
  if (g.size() <= maxAll) return cbe::allSources(g);
  std::set<int> chosen;
  while (static_cast<int>(chosen.size()) < sample)
    chosen.insert(static_cast<int>(rng.below(static_cast<std::uint64_t>(g.size()))));
  return std::vector<int>(chosen.begin(), chosen.end());
}

// The solvers every property is checked against. Phase 3 appends the mutable
// Dijkstra here, so the same properties also judge the mutants.
const std::vector<const cbe::Solver*>& solversUnderTest();
const std::vector<const cbe::Solver*>& referenceSolvers();

}  // namespace cbe_test
