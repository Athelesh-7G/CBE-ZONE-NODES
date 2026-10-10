// The "unit under test" for mutation testing: a C++ Dijkstra written in the same
// style as lib/dijkstra.ts (lazy deletion, early exit once the target is settled),
// with a runtime-selectable injected bug (Mutant). Mutant::None is the correct code.
//
// Why a runtime enum rather than a template policy or #ifdef: one binary can run the
// whole suite against every mutant in-process, with no recompilation, and each fault
// is a single labelled branch next to the line it corrupts. The cost is a few extra
// branches in a test-only copy of the algorithm.
//
// This is deliberately NOT the reference Dijkstra in solvers.hpp: the reference must
// stay correct, the UUT is the thing we break.
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "cbe/graph.hpp"
#include "cbe/solvers.hpp"

namespace cbe {

enum class Mutant {
  None,
  RelaxLessEqual,         // relax with <= instead of <
  SkipLastNeighbor,       // never relax a node's last adjacency entry
  StaleCheckLoose,        // stale test off by one: d > dist[u] + 1 instead of d > dist[u]
  IgnoreReverseDirection, // build adjacency in the u->v direction only
  WrongInitialDistance,   // dist[source] = 1 instead of 0
  WrongPredecessor,       // record the grandparent as the predecessor
  EarlyExitTooEarly,      // stop as soon as the target is first reached, not settled
  Int32Overflow,          // distances wrap at 2^32
  NoStaleSkip,            // never skip stale queue entries
  WrongHeapOrder,         // max-heap instead of min-heap (forgot std::greater)
  ZeroWeightSkip,         // treat zero-weight edges as absent
  KeepFirstParallelEdge,  // with parallel edges keep the first, not the cheapest
};

struct MutantInfo {
  Mutant id;
  const char* name;
  const char* description;
  const char* equivalentNote;  // non-empty: documented as an equivalent mutant
};

inline const std::vector<MutantInfo>& allMutants() {
  static const std::vector<MutantInfo> m = {
      {Mutant::RelaxLessEqual, "relax-le", "relax with <= instead of <", ""},
      {Mutant::SkipLastNeighbor, "skip-last-neighbor", "never relax the last neighbor of a node", ""},
      {Mutant::StaleCheckLoose, "stale-check-off-by-one", "stale test d > dist[u] + 1", ""},
      {Mutant::IgnoreReverseDirection, "one-direction-only", "adjacency built u->v only", ""},
      {Mutant::WrongInitialDistance, "wrong-initial-distance", "dist[source] starts at 1", ""},
      {Mutant::WrongPredecessor, "wrong-predecessor", "predecessor set to the grandparent", ""},
      {Mutant::EarlyExitTooEarly, "early-exit-too-early", "stop when target first reached", ""},
      {Mutant::Int32Overflow, "int32-overflow", "distance arithmetic wraps at 2^32", ""},
      {Mutant::NoStaleSkip, "no-stale-skip", "stale queue entries are expanded", ""},
      {Mutant::WrongHeapOrder, "wrong-heap-order", "max-heap used as the queue", ""},
      {Mutant::ZeroWeightSkip, "zero-weight-skip", "zero-weight edges ignored", ""},
      {Mutant::KeepFirstParallelEdge, "keep-first-parallel-edge", "first parallel edge kept, not cheapest", ""},
  };
  return m;
}

inline const char* mutantName(Mutant m) {
  if (m == Mutant::None) return "none";
  for (const MutantInfo& i : allMutants())
    if (i.id == m) return i.name;
  return "?";
}

// The mutant the whole test suite currently runs against (set by the mutation runner).
inline Mutant& activeMutant() {
  static Mutant m = Mutant::None;
  return m;
}

// Thrown when a (mutated) run exceeds its work budget, i.e. it would not terminate
// in reasonable time. A hang is a failure, not a test-suite hang.
class BudgetExceeded : public std::runtime_error {
 public:
  BudgetExceeded() : std::runtime_error("work budget exceeded (non-terminating or runaway search)") {}
};

struct UutStats {
  std::size_t pops = 0;        // queue pops, including stale ones
  std::size_t expansions = 0;  // nodes whose neighbors were relaxed
};

// Shortest path from s. With target >= 0 the search stops early (the real code's
// behavior); with target == -1 it runs to completion and fills every node.
struct UutOutput {
  std::vector<Weight> dist;
  std::vector<int> pred;
  UutStats stats;
};

inline UutOutput dijkstraUutRaw(const Graph& g, int s, int target, Mutant m) {
  const std::size_t n = ix(g.size());
  // Adjacency, possibly corrupted by a mutant.
  std::vector<std::vector<Arc>> adj(n);
  if (m == Mutant::IgnoreReverseDirection) {
    for (const Edge& e : g.edges()) adj[ix(e.u)].push_back({e.v, e.w});
  } else {
    for (int u = 0; u < g.size(); ++u) adj[ix(u)] = g.arcs(u);
  }
  if (m == Mutant::KeepFirstParallelEdge) {
    for (auto& list : adj) {
      std::set<int> seen;
      std::vector<Arc> kept;
      for (const Arc& a : list)
        if (seen.insert(a.to).second) kept.push_back(a);
      list = kept;
    }
  }

  UutOutput out;
  out.dist.assign(n, kInf);
  out.pred.assign(n, -1);
  using Item = std::pair<Weight, int>;
  std::vector<Item> heap;
  auto minFirst = [](const Item& a, const Item& b) { return a > b; };  // min-heap order
  auto maxFirst = [](const Item& a, const Item& b) { return a < b; };  // the bug
  auto push = [&](Item it) {
    heap.push_back(it);
    if (m == Mutant::WrongHeapOrder) std::push_heap(heap.begin(), heap.end(), maxFirst);
    else std::push_heap(heap.begin(), heap.end(), minFirst);
  };
  auto pop = [&]() {
    if (m == Mutant::WrongHeapOrder) std::pop_heap(heap.begin(), heap.end(), maxFirst);
    else std::pop_heap(heap.begin(), heap.end(), minFirst);
    const Item it = heap.back();
    heap.pop_back();
    return it;
  };

  out.dist[ix(s)] = m == Mutant::WrongInitialDistance ? 1 : 0;
  push({out.dist[ix(s)], s});
  const std::size_t budget = 50 * (n + 2 * g.edgeCount()) + 1000;

  while (!heap.empty()) {
    if (++out.stats.pops > budget) throw BudgetExceeded();
    const Item top = pop();
    const int u = top.second;
    if (m == Mutant::StaleCheckLoose) {
      if (top.first > out.dist[ix(u)] + 1) continue;
    } else if (m != Mutant::NoStaleSkip) {
      if (top.first > out.dist[ix(u)]) continue;  // stale entry
    }
    ++out.stats.expansions;
    if (u == target && m != Mutant::EarlyExitTooEarly) break;  // target settled

    const std::vector<Arc>& list = adj[ix(u)];
    for (std::size_t k = 0; k < list.size(); ++k) {
      if (m == Mutant::SkipLastNeighbor && k + 1 == list.size()) continue;
      const Arc& arc = list[k];
      if (m == Mutant::ZeroWeightSkip && arc.w == 0) continue;
      Weight cand = out.dist[ix(u)] + arc.w;
      if (m == Mutant::Int32Overflow) cand = static_cast<Weight>(static_cast<std::uint32_t>(cand));
      const bool better = m == Mutant::RelaxLessEqual ? cand <= out.dist[ix(arc.to)]
                                                       : cand < out.dist[ix(arc.to)];
      if (better) {
        out.dist[ix(arc.to)] = cand;
        out.pred[ix(arc.to)] =
            (m == Mutant::WrongPredecessor && out.pred[ix(u)] != -1) ? out.pred[ix(u)] : u;
        push({cand, arc.to});
      }
    }
    if (m == Mutant::EarlyExitTooEarly && target >= 0 && out.dist[ix(target)] < kInf) break;
  }
  return out;
}

inline Result uutResult(const Graph& g, const UutOutput& o, int t) {
  Result r;
  if (o.dist[ix(t)] >= kInf) return r;
  r.reachable = true;
  r.distance = o.dist[ix(t)];
  int at = t;
  for (int guard = 0; at != -1 && guard <= g.size(); ++guard) {
    r.path.push_back(at);
    at = o.pred[ix(at)];
  }
  std::reverse(r.path.begin(), r.path.end());
  return r;
}

// Answers one (source, target) query with the early-exit search.
inline Result dijkstraUut(const Graph& g, int s, int t, Mutant m) {
  return uutResult(g, dijkstraUutRaw(g, s, t, m), t);
}

// As a Solver for the differential engine and the property tests. For each source it
// runs one full search for every target, then re-answers up to `sampledTargets`
// targets with the early-exit search (the code path the real implementation uses),
// so the early-exit logic is judged without paying one search per target on big graphs.
class UutSolver : public Solver {
 public:
  explicit UutSolver(std::size_t sampledTargets = 10) : sampled_(sampledTargets) {}
  std::string name() const override { return std::string("uut(") + mutantName(activeMutant()) + ")"; }

  std::vector<std::vector<Result>> solve(const Graph& g, const std::vector<int>& sources) const override {
    const Mutant m = activeMutant();
    std::vector<std::vector<Result>> out;
    for (int s : sources) {
      const UutOutput full = dijkstraUutRaw(g, s, -1, m);
      std::vector<Result> row;
      for (int t = 0; t < g.size(); ++t) row.push_back(uutResult(g, full, t));
      const std::size_t n = ix(g.size());
      const std::size_t k = std::min(sampled_, n);
      for (std::size_t i = 0; i < k; ++i) {
        const std::size_t t = (i * n) / k;  // evenly spread, deterministic
        row[t] = dijkstraUut(g, s, static_cast<int>(t), m);
      }
      out.push_back(std::move(row));
    }
    return out;
  }

 private:
  std::size_t sampled_;
};

}  // namespace cbe
