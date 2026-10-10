// Seeded random graph generation.
#pragma once

#include <algorithm>
#include <string>
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

// ---------------------------------------------------------------------------
// Shaped graphs, including adversarial ones. generateCase() draws a shape, a size
// and a weight regime from the Rng, so a single seed describes the whole case.
// ---------------------------------------------------------------------------
namespace cbe {

enum class Shape { Random, Sparse, Disconnected, Chain, Cycle, Star, Complete, Grid, Layered };

inline const char* shapeName(Shape s) {
  switch (s) {
    case Shape::Random: return "random";
    case Shape::Sparse: return "sparse";
    case Shape::Disconnected: return "disconnected";
    case Shape::Chain: return "chain";
    case Shape::Cycle: return "cycle";
    case Shape::Star: return "star";
    case Shape::Complete: return "complete";
    case Shape::Grid: return "grid";
    case Shape::Layered: return "layered";
  }
  return "?";
}

struct WeightRegime {
  Weight maxWeight = 20;
  double pZero = 0.0;
  double pParallel = 0.0;
  const char* label = "small";
};

inline WeightRegime drawRegime(Rng& rng) {
  switch (rng.below(5)) {
    case 0: return {3, 0.0, 0.0, "ties(1..3)"};
    case 1: return {100, 0.0, 0.0, "medium(1..100)"};
    case 2: return {20, 0.2, 0.0, "zeros"};
    case 3: return {50, 0.1, 0.25, "zeros+parallel"};
    default: return {kMaxWeight, 0.0, 0.1, "huge(1..1e9)"};
  }
}

inline Weight regimeWeight(Rng& rng, const WeightRegime& r) {
  if (r.pZero > 0 && rng.chance(r.pZero)) return 0;
  return rng.range(1, r.maxWeight);
}

// Adds a parallel copy of some edges, per the regime.
inline void addParallel(Rng& rng, Graph& g, const WeightRegime& r) {
  if (r.pParallel <= 0) return;
  const std::vector<Edge> snapshot = g.edges();
  for (const Edge& e : snapshot)
    if (rng.chance(r.pParallel)) g.addEdge(e.u, e.v, regimeWeight(rng, r));
}

struct GeneratedCase {
  Graph graph;
  std::string label;  // human-readable description for failure messages
};

inline GeneratedCase generateCase(Rng& rng, int maxN) {
  const Shape shape = static_cast<Shape>(rng.below(9));
  const WeightRegime reg = drawRegime(rng);
  int n = static_cast<int>(rng.range(2, maxN));
  Graph g;
  switch (shape) {
    case Shape::Random:
    case Shape::Sparse:
    case Shape::Disconnected: {
      GenParams p;
      p.n = n;
      p.density = shape == Shape::Random ? 0.05 + 0.5 * rng.unit()
                                         : 2.0 / static_cast<double>(n) + 0.02 * rng.unit();
      p.connected = shape != Shape::Disconnected;
      p.maxWeight = reg.maxWeight;
      p.pZeroWeight = reg.pZero;
      p.pParallel = reg.pParallel;
      g = randomGraph(rng, p);
      break;
    }
    case Shape::Chain:
    case Shape::Cycle: {
      g = Graph(n);
      for (int i = 0; i + 1 < n; ++i) g.addEdge(i, i + 1, regimeWeight(rng, reg));
      if (shape == Shape::Cycle && n > 2) g.addEdge(n - 1, 0, regimeWeight(rng, reg));
      addParallel(rng, g, reg);
      break;
    }
    case Shape::Star: {
      g = Graph(n);
      for (int i = 1; i < n; ++i) g.addEdge(0, i, regimeWeight(rng, reg));
      addParallel(rng, g, reg);
      break;
    }
    case Shape::Complete: {
      n = std::min(n, 40);
      g = Graph(n);
      for (int u = 0; u < n; ++u)
        for (int v = u + 1; v < n; ++v) g.addEdge(u, v, regimeWeight(rng, reg));
      break;
    }
    case Shape::Grid: {
      const int rows = std::max(1, static_cast<int>(rng.range(1, 14)));
      const int cols = std::max(2, std::min(14, n / rows + 1));
      n = rows * cols;
      g = Graph(n);
      for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
          if (c + 1 < cols) g.addEdge(r * cols + c, r * cols + c + 1, regimeWeight(rng, reg));
          if (r + 1 < rows) g.addEdge(r * cols + c, (r + 1) * cols + c, regimeWeight(rng, reg));
        }
      addParallel(rng, g, reg);
      break;
    }
    case Shape::Layered: {
      // Dense bipartite layers: many equal-length alternatives, so many ties and
      // many stale queue entries.
      const int layers = std::max(2, std::min(8, n / 3));
      const int width = std::max(2, n / layers);
      n = layers * width;
      g = Graph(n);
      for (int l = 0; l + 1 < layers; ++l)
        for (int a = 0; a < width; ++a)
          for (int b = 0; b < width; ++b)
            g.addEdge(l * width + a, (l + 1) * width + b, regimeWeight(rng, reg));
      break;
    }
  }
  // The label is built before g is moved from.
  std::string label = std::string(shapeName(shape)) + "/" + reg.label + "/n=" +
                      std::to_string(n) + "/m=" + std::to_string(g.edgeCount());
  return {std::move(g), std::move(label)};
}

}  // namespace cbe
