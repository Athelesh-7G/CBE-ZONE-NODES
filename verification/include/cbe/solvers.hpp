// Three independent reference solvers. Each is written from the textbook
// definition and shares no code with the others (not even path reconstruction),
// so a bug in one is unlikely to be repeated in another.
//
//   DijkstraSolver       std::priority_queue (min-heap), stale-entry skipping
//   BellmanFordSolver    V-1 relaxation passes + one extra pass for negative cycles
//   FloydWarshallSolver  all-pairs dynamic programming with a next-hop matrix
#pragma once

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cbe/graph.hpp"

namespace cbe {

// A solver answers "shortest path from each of these sources to every target".
// out[i][t] is the result from sources[i] to node t.
class Solver {
 public:
  virtual ~Solver() = default;
  virtual std::string name() const = 0;
  virtual std::vector<std::vector<Result>> solve(
      const Graph& g, const std::vector<int>& sources) const = 0;
};

inline std::size_t ix(int i) { return static_cast<std::size_t>(i); }

// ---------------------------------------------------------------------------
// Dijkstra with std::priority_queue and lazy deletion.
// ---------------------------------------------------------------------------
class DijkstraSolver : public Solver {
 public:
  std::string name() const override { return "dijkstra"; }

  std::vector<std::vector<Result>> solve(
      const Graph& g, const std::vector<int>& sources) const override {
    std::vector<std::vector<Result>> out;
    for (int s : sources) out.push_back(fromSource(g, s));
    return out;
  }

 private:
  static std::vector<Result> fromSource(const Graph& g, int s) {
    const std::size_t n = ix(g.size());
    std::vector<Weight> dist(n, kInf);
    std::vector<int> pred(n, -1);
    using Item = std::pair<Weight, int>;  // (distance, node)
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    dist[ix(s)] = 0;
    pq.push({0, s});
    while (!pq.empty()) {
      const Item top = pq.top();
      pq.pop();
      const int u = top.second;
      if (top.first > dist[ix(u)]) continue;  // stale entry
      for (const Arc& arc : g.arcs(u)) {
        const Weight cand = top.first + arc.w;
        if (cand < dist[ix(arc.to)]) {
          dist[ix(arc.to)] = cand;
          pred[ix(arc.to)] = u;
          pq.push({cand, arc.to});
        }
      }
    }
    std::vector<Result> res(n);
    for (int t = 0; t < g.size(); ++t) {
      if (dist[ix(t)] >= kInf) continue;
      Result& r = res[ix(t)];
      r.reachable = true;
      r.distance = dist[ix(t)];
      for (int at = t; at != -1; at = pred[ix(at)]) r.path.push_back(at);
      std::reverse(r.path.begin(), r.path.end());
    }
    return res;
  }
};

// ---------------------------------------------------------------------------
// Bellman-Ford on a directed edge list (weights may be negative). Detects
// negative cycles that are reachable from the source.
// ---------------------------------------------------------------------------
struct DirectedEdge {
  int from;
  int to;
  Weight w;
};

struct BellmanFordOutput {
  std::vector<Weight> dist;
  std::vector<int> pred;
  bool negativeCycle = false;
};

inline BellmanFordOutput bellmanFordDirected(int n, const std::vector<DirectedEdge>& edges,
                                             int source) {
  BellmanFordOutput out;
  out.dist.assign(ix(n), kInf);
  out.pred.assign(ix(n), -1);
  out.dist[ix(source)] = 0;
  auto relax = [&out](const DirectedEdge& e) {
    const Weight du = out.dist[ix(e.from)];
    if (du >= kInf) return false;
    if (du + e.w < out.dist[ix(e.to)]) {
      out.dist[ix(e.to)] = du + e.w;
      out.pred[ix(e.to)] = e.from;
      return true;
    }
    return false;
  };
  for (int pass = 1; pass < n; ++pass) {
    bool changed = false;
    for (const DirectedEdge& e : edges) changed = relax(e) || changed;
    if (!changed) break;
  }
  // One more pass: any further improvement proves a negative cycle.
  for (const DirectedEdge& e : edges) {
    const Weight du = out.dist[ix(e.from)];
    if (du < kInf && du + e.w < out.dist[ix(e.to)]) {
      out.negativeCycle = true;
      break;
    }
  }
  return out;
}

class BellmanFordSolver : public Solver {
 public:
  std::string name() const override { return "bellman-ford"; }

  std::vector<std::vector<Result>> solve(
      const Graph& g, const std::vector<int>& sources) const override {
    std::vector<DirectedEdge> directed;
    for (const Edge& e : g.edges()) {
      directed.push_back({e.u, e.v, e.w});
      directed.push_back({e.v, e.u, e.w});
    }
    std::vector<std::vector<Result>> out;
    for (int s : sources) {
      const BellmanFordOutput bf = bellmanFordDirected(g.size(), directed, s);
      if (bf.negativeCycle)  // impossible with non-negative weights
        throw std::logic_error("negative cycle on a non-negative graph");
      std::vector<Result> res(ix(g.size()));
      for (int t = 0; t < g.size(); ++t) {
        if (bf.dist[ix(t)] >= kInf) continue;
        Result& r = res[ix(t)];
        r.reachable = true;
        r.distance = bf.dist[ix(t)];
        std::vector<int> rev;
        int at = t;
        // The guard bounds the walk, so a corrupt predecessor chain cannot hang.
        for (int guard = 0; at != -1 && guard <= g.size(); ++guard) {
          rev.push_back(at);
          at = bf.pred[ix(at)];
        }
        r.path.assign(rev.rbegin(), rev.rend());
      }
      out.push_back(std::move(res));
    }
    return out;
  }
};

// ---------------------------------------------------------------------------
// Floyd-Warshall: computes the whole matrix once, then answers every source.
// ---------------------------------------------------------------------------
class FloydWarshallSolver : public Solver {
 public:
  std::string name() const override { return "floyd-warshall"; }

  std::vector<std::vector<Result>> solve(
      const Graph& g, const std::vector<int>& sources) const override {
    const std::size_t n = ix(g.size());
    std::vector<Weight> d(n * n, kInf);
    std::vector<int> next(n * n, -1);  // first hop from i toward j
    auto at = [n](std::size_t i, std::size_t j) { return i * n + j; };
    for (std::size_t i = 0; i < n; ++i) {
      d[at(i, i)] = 0;
      next[at(i, i)] = static_cast<int>(i);
    }
    for (const Edge& e : g.edges()) {
      const std::size_t u = ix(e.u);
      const std::size_t v = ix(e.v);
      if (e.w < d[at(u, v)]) {
        d[at(u, v)] = d[at(v, u)] = e.w;
        next[at(u, v)] = e.v;
        next[at(v, u)] = e.u;
      }
    }
    for (std::size_t k = 0; k < n; ++k)
      for (std::size_t i = 0; i < n; ++i) {
        if (d[at(i, k)] >= kInf) continue;
        for (std::size_t j = 0; j < n; ++j) {
          if (d[at(k, j)] >= kInf) continue;
          if (d[at(i, k)] + d[at(k, j)] < d[at(i, j)]) {
            d[at(i, j)] = d[at(i, k)] + d[at(k, j)];
            next[at(i, j)] = next[at(i, k)];
          }
        }
      }
    std::vector<std::vector<Result>> out;
    for (int s : sources) {
      const std::size_t si = ix(s);
      std::vector<Result> res(n);
      for (std::size_t t = 0; t < n; ++t) {
        if (d[at(si, t)] >= kInf) continue;
        res[t].reachable = true;
        res[t].distance = d[at(si, t)];
        std::size_t cur = si;
        res[t].path.push_back(s);
        for (std::size_t guard = 0; cur != t && guard <= n; ++guard) {
          cur = ix(next[at(cur, t)]);
          res[t].path.push_back(static_cast<int>(cur));
        }
      }
      out.push_back(std::move(res));
    }
    return out;
  }
};

// Adapter so any callable (for example the mutable Dijkstra under test) can take
// part in differential runs. `fn` answers one (source, target) query.
class FunctionSolver : public Solver {
 public:
  using Fn = std::function<Result(const Graph&, int, int)>;
  FunctionSolver(std::string name, Fn fn) : name_(std::move(name)), fn_(std::move(fn)) {}
  std::string name() const override { return name_; }
  std::vector<std::vector<Result>> solve(
      const Graph& g, const std::vector<int>& sources) const override {
    std::vector<std::vector<Result>> out;
    for (int s : sources) {
      std::vector<Result> row;
      for (int t = 0; t < g.size(); ++t) row.push_back(fn_(g, s, t));
      out.push_back(std::move(row));
    }
    return out;
  }

 private:
  std::string name_;
  Fn fn_;
};

}  // namespace cbe
