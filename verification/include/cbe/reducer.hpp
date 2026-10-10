// Test-case reduction (delta debugging, Zeller's ddmin) for failing graphs.
//
// Given a graph and a query (s, t) on which `stillFails` is true, produce a smaller
// graph/query that still fails:
//   1. ddmin over the edge list (remove chunks of edges, then single edges),
//   2. drop nodes no edge touches (keeping s and t) and renumber,
//   3. simplify weights toward 0/1 while the failure persists.
// The result is 1-minimal for edges: removing any single remaining edge makes the
// failure disappear. It is not guaranteed to be globally minimal (ddmin is greedy).
#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include "cbe/graph.hpp"

namespace cbe {

using FailurePredicate = std::function<bool(const Graph&, int s, int t)>;

struct ReducedCase {
  Graph graph;
  int s = 0;
  int t = 0;
  std::size_t predicateCalls = 0;
};

namespace detail {

inline Graph buildFrom(int n, const std::vector<Edge>& edges) {
  Graph g(n);
  for (const Edge& e : edges) g.addEdge(e.u, e.v, e.w);
  return g;
}

}  // namespace detail

inline ReducedCase reduceFailure(const Graph& original, int s, int t, const FailurePredicate& stillFails) {
  ReducedCase out;
  out.s = s;
  out.t = t;
  const int n = original.size();
  std::vector<Edge> edges = original.edges();
  auto fails = [&](const std::vector<Edge>& es) {
    ++out.predicateCalls;
    return stillFails(detail::buildFrom(n, es), s, t);
  };

  // ---- 1. ddmin over edges --------------------------------------------------
  std::size_t granularity = 2;
  while (edges.size() >= 2) {
    const std::size_t chunk = std::max<std::size_t>(1, (edges.size() + granularity - 1) / granularity);
    bool reduced = false;
    for (std::size_t start = 0; start < edges.size(); start += chunk) {
      // Try removing this chunk (the "complement" test of ddmin).
      std::vector<Edge> rest;
      for (std::size_t i = 0; i < edges.size(); ++i)
        if (i < start || i >= start + chunk) rest.push_back(edges[i]);
      if (rest.size() < edges.size() && fails(rest)) {
        edges = std::move(rest);
        granularity = std::max<std::size_t>(granularity - 1, 2);
        reduced = true;
        break;
      }
    }
    if (!reduced) {
      if (granularity >= edges.size()) break;
      granularity = std::min(edges.size(), granularity * 2);
    }
  }
  // Single-edge pass: guarantees 1-minimality.
  for (bool progress = true; progress;) {
    progress = false;
    for (std::size_t i = 0; i < edges.size(); ++i) {
      std::vector<Edge> rest = edges;
      rest.erase(rest.begin() + static_cast<std::ptrdiff_t>(i));
      if (fails(rest)) {
        edges = std::move(rest);
        progress = true;
        break;
      }
    }
  }

  // ---- 2. drop untouched nodes and renumber --------------------------------
  std::vector<int> newId(static_cast<std::size_t>(n), -1);
  std::vector<char> used(static_cast<std::size_t>(n), 0);
  used[static_cast<std::size_t>(s)] = used[static_cast<std::size_t>(t)] = 1;
  for (const Edge& e : edges) used[static_cast<std::size_t>(e.u)] = used[static_cast<std::size_t>(e.v)] = 1;
  int next = 0;
  for (int i = 0; i < n; ++i)
    if (used[static_cast<std::size_t>(i)]) newId[static_cast<std::size_t>(i)] = next++;
  std::vector<Edge> compact;
  for (const Edge& e : edges)
    compact.push_back({newId[static_cast<std::size_t>(e.u)], newId[static_cast<std::size_t>(e.v)], e.w});
  const int ns = newId[static_cast<std::size_t>(s)];
  const int nt = newId[static_cast<std::size_t>(t)];
  ++out.predicateCalls;
  if (!stillFails(detail::buildFrom(next, compact), ns, nt)) {
    // Renumbering changed behavior (an order-dependent bug). Keep the unrenumbered case.
    out.graph = detail::buildFrom(n, edges);
    return out;
  }

  // ---- 3. simplify weights --------------------------------------------------
  auto failsCompact = [&](const std::vector<Edge>& es) {
    ++out.predicateCalls;
    return stillFails(detail::buildFrom(next, es), ns, nt);
  };
  for (std::size_t i = 0; i < compact.size(); ++i) {
    for (Weight candidate : {Weight{1}, Weight{0}, compact[i].w / 2}) {
      if (candidate >= compact[i].w) continue;
      std::vector<Edge> trial = compact;
      trial[i].w = candidate;
      if (failsCompact(trial)) {
        compact = std::move(trial);
        break;
      }
    }
  }
  out.graph = detail::buildFrom(next, compact);
  out.s = ns;
  out.t = nt;
  return out;
}

}  // namespace cbe
