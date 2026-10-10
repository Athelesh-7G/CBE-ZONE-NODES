// Seeded random graph generation.
#pragma once

#include <algorithm>
#include <vector>

#include "cbe/graph.hpp"
#include "cbe/rng.hpp"

namespace cbe {

struct GenParams {
  int n = 8;
  double density = 0.3;      // chance of an extra edge per node pair
  bool connected = true;     // start from a random spanning tree
  Weight maxWeight = 20;     // weights are drawn from [0 or 1, maxWeight]
  double pZeroWeight = 0.0;  // chance an edge has weight 0
  double pParallel = 0.0;    // chance an edge is duplicated with another weight
};

inline Weight drawWeight(Rng& rng, const GenParams& p) {
  if (p.pZeroWeight > 0 && rng.chance(p.pZeroWeight)) return 0;
  return rng.range(1, p.maxWeight);
}

inline Graph randomGraph(Rng& rng, const GenParams& p) {
  Graph g(p.n);
  if (p.connected) {
    for (int v = 1; v < p.n; ++v)
      g.addEdge(static_cast<int>(rng.below(static_cast<std::uint64_t>(v))), v,
                drawWeight(rng, p));
  }
  for (int u = 0; u < p.n; ++u)
    for (int v = u + 1; v < p.n; ++v)
      if (rng.chance(p.density)) g.addEdge(u, v, drawWeight(rng, p));
  if (p.pParallel > 0) {
    const std::vector<Edge> snapshot = g.edges();
    for (const Edge& e : snapshot)
      if (rng.chance(p.pParallel)) g.addEdge(e.u, e.v, drawWeight(rng, p));
  }
  return g;
}

}  // namespace cbe
