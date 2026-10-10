// Helpers that connect the mutable Dijkstra, the references and the reducer.
#pragma once

#include <optional>

#include "cbe/differential.hpp"
#include "cbe/reducer.hpp"
#include "cbe/solvers.hpp"
#include "cbe/uut.hpp"

namespace cbe {

// True when the UUT (with mutant `m`) gives a wrong or invalid answer for (s, t),
// judged against Bellman-Ford. A runaway search counts as a failure. Failures that
// are visible only in internal statistics (extra expansions) are not detected here.
inline bool uutFails(const Graph& g, int s, int t, Mutant m) {
  try {
    const Result got = dijkstraUut(g, s, t, m);
    const Result want = BellmanFordSolver().solve(g, {s})[0][ix(t)];
    if (got.reachable != want.reachable) return true;
    if (got.reachable && got.distance != want.distance) return true;
    return !validatePath(g, got, s, t).empty();
  } catch (const BudgetExceeded&) {
    return true;
  }
}

// First (s, t) on which the mutant visibly fails, scanning sources in order.
inline std::optional<Query> findFailingQuery(const Graph& g, Mutant m) {
  for (int s = 0; s < g.size(); ++s)
    for (int t = 0; t < g.size(); ++t)
      if (uutFails(g, s, t, m)) return Query{s, t};
  return std::nullopt;
}

}  // namespace cbe
