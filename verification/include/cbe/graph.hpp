// Graph type, text I/O format and Result type for the verification toolchain.
//
// Model: undirected, non-negative integer weights, node ids 0..n-1. Parallel
// edges and zero weights are allowed (the TypeScript implementation accepts both);
// self-loops are rejected. Weights are capped at kMaxWeight so that path sums on
// any graph used here stay far below kInf (no overflow in the reference solvers).
#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <string>
#include <vector>

namespace cbe {

using Weight = std::int64_t;
inline constexpr Weight kInf = std::numeric_limits<Weight>::max() / 4;
inline constexpr Weight kMaxWeight = 1'000'000'000;

struct Arc {
  int to;
  Weight w;
};

struct Edge {
  int u;
  int v;
  Weight w;
};

struct Query {
  int s;
  int t;
};

class Graph {
 public:
  explicit Graph(int n = 0) {
    if (n < 0) throw std::invalid_argument("negative node count");
    adj_.resize(static_cast<std::size_t>(n));
  }

  int size() const { return static_cast<int>(adj_.size()); }
  std::size_t edgeCount() const { return edges_.size(); }
  const std::vector<Edge>& edges() const { return edges_; }
  const std::vector<Arc>& arcs(int u) const { return adj_[static_cast<std::size_t>(u)]; }

  void addEdge(int u, int v, Weight w) {
    if (u < 0 || v < 0 || u >= size() || v >= size())
      throw std::invalid_argument("edge endpoint out of range");
    if (u == v) throw std::invalid_argument("self-loop");
    if (w < 0 || w > kMaxWeight) throw std::invalid_argument("weight out of range");
    edges_.push_back({u, v, w});
    adj_[static_cast<std::size_t>(u)].push_back({v, w});
    adj_[static_cast<std::size_t>(v)].push_back({u, w});
  }

  // Cheapest edge between a and b, or kInf if there is none.
  Weight minEdge(int a, int b) const {
    Weight best = kInf;
    for (const Arc& arc : arcs(a))
      if (arc.to == b && arc.w < best) best = arc.w;
    return best;
  }

 private:
  std::vector<Edge> edges_;
  std::vector<std::vector<Arc>> adj_;
};

struct Result {
  bool reachable = false;
  Weight distance = kInf;  // kInf when unreachable
  std::vector<int> path;   // source..target, empty when unreachable
};

// ---------------------------------------------------------------------------
// Text format. A file is a sequence of blocks; '#' starts a comment line.
//
//   graph <name>
//   <n> <m> <q>
//   u v w          (m lines)
//   s t            (q lines: queries)
// ---------------------------------------------------------------------------
struct Block {
  std::string name;
  Graph graph;
  std::vector<Query> queries;
};

inline void writeBlock(std::ostream& os, const std::string& name, const Graph& g,
                       const std::vector<Query>& queries) {
  os << "graph " << name << "\n"
     << g.size() << " " << g.edgeCount() << " " << queries.size() << "\n";
  for (const Edge& e : g.edges()) os << e.u << " " << e.v << " " << e.w << "\n";
  for (const Query& q : queries) os << q.s << " " << q.t << "\n";
}

inline std::vector<Block> parseBlocks(std::istream& in) {
  std::vector<std::pair<int, std::string>> lines;
  std::string line;
  for (int no = 1; std::getline(in, line); ++no) {
    std::size_t first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos || line[first] == '#') continue;
    lines.emplace_back(no, line);
  }
  auto fail = [](int no, const std::string& msg) {
    throw std::runtime_error("graph file line " + std::to_string(no) + ": " + msg);
  };
  std::vector<Block> blocks;
  std::size_t i = 0;
  while (i < lines.size()) {
    std::istringstream head(lines[i].second);
    std::string kw, name;
    head >> kw >> name;
    if (kw != "graph" || name.empty()) fail(lines[i].first, "expected 'graph <name>'");
    ++i;
    if (i >= lines.size()) fail(lines[i - 1].first, "missing counts line");
    long long n = -1, m = -1, q = -1;
    std::istringstream counts(lines[i].second);
    if (!(counts >> n >> m >> q) || n < 0 || m < 0 || q < 0)
      fail(lines[i].first, "expected '<n> <m> <q>'");
    ++i;
    Block b{name, Graph(static_cast<int>(n)), {}};
    for (long long k = 0; k < m; ++k, ++i) {
      if (i >= lines.size()) fail(lines.back().first, "truncated edge list");
      std::istringstream es(lines[i].second);
      long long u, v, w;
      if (!(es >> u >> v >> w)) fail(lines[i].first, "expected 'u v w'");
      try {
        b.graph.addEdge(static_cast<int>(u), static_cast<int>(v), w);
      } catch (const std::invalid_argument& e) {
        fail(lines[i].first, e.what());
      }
    }
    for (long long k = 0; k < q; ++k, ++i) {
      if (i >= lines.size()) fail(lines.back().first, "truncated query list");
      std::istringstream qs(lines[i].second);
      long long s, t;
      if (!(qs >> s >> t) || s < 0 || t < 0 || s >= n || t >= n)
        fail(lines[i].first, "expected 'source target' within range");
      b.queries.push_back({static_cast<int>(s), static_cast<int>(t)});
    }
    blocks.push_back(std::move(b));
  }
  return blocks;
}

}  // namespace cbe
