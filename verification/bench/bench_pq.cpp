// Benchmark: sorted-array priority queue vs binary heap inside the same Dijkstra.
//
//   cbe_bench [--max-v N] [--quick true]
//
// Three queues, identical algorithm (lazy deletion), identical graphs:
//   heap       std::priority_queue (binary heap, min-first)
//   sorted-asc a sorted vector kept ascending, pop = erase(begin)   <- what
//              lib/dijkstra.ts does (binary-search insert, Array.shift pop)
//   sorted-desc a sorted vector kept descending, pop = pop_back     <- the same idea
//              done the cheap way (O(1) pop, O(n) insert)
//
// This measures C++ on this machine. It says nothing certain about V8's Array.splice
// and Array.shift, which have their own costs; see verification/README.md.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "cbe/graph.hpp"
#include "cbe/rng.hpp"
#include "cbe/solvers.hpp"

namespace {

using cbe::Weight;
using Item = std::pair<Weight, int>;

struct Csr {  // compressed adjacency, so the graph layout is not part of the comparison
  std::vector<std::size_t> start;
  std::vector<int> to;
  std::vector<Weight> w;
  int n = 0;
  std::size_t edges = 0;
};

Csr toCsr(const cbe::Graph& g) {
  Csr c;
  c.n = g.size();
  c.edges = g.edgeCount();
  c.start.assign(static_cast<std::size_t>(c.n) + 1, 0);
  for (int u = 0; u < c.n; ++u) c.start[static_cast<std::size_t>(u) + 1] = c.start[static_cast<std::size_t>(u)] + g.arcs(u).size();
  for (int u = 0; u < c.n; ++u)
    for (const cbe::Arc& a : g.arcs(u)) {
      c.to.push_back(a.to);
      c.w.push_back(a.w);
    }
  return c;
}

struct HeapPQ {
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
  void push(Item i) { q.push(i); }
  Item pop() { Item t = q.top(); q.pop(); return t; }
  bool empty() const { return q.empty(); }
  std::size_t size() const { return q.size(); }
};

struct SortedAscPQ {  // ascending; equal keys go after existing ones (FIFO), like the TypeScript
  std::vector<Item> v;
  void push(Item i) { v.insert(std::upper_bound(v.begin(), v.end(), i, [](const Item& a, const Item& b) { return a.first < b.first; }), i); }
  Item pop() { Item t = v.front(); v.erase(v.begin()); return t; }
  bool empty() const { return v.empty(); }
  std::size_t size() const { return v.size(); }
};

struct SortedDescPQ {  // descending; the minimum is at the back
  std::vector<Item> v;
  void push(Item i) { v.insert(std::upper_bound(v.begin(), v.end(), i, [](const Item& a, const Item& b) { return a.first > b.first; }), i); }
  Item pop() { Item t = v.back(); v.pop_back(); return t; }
  bool empty() const { return v.empty(); }
  std::size_t size() const { return v.size(); }
};

struct RunStats {
  Weight checksum = 0;  // sum of finite distances, to prove all queues agree
  std::size_t maxQueue = 0;
};

template <class PQ>
RunStats dijkstra(const Csr& g, int s) {
  std::vector<Weight> dist(static_cast<std::size_t>(g.n), cbe::kInf);
  PQ pq;
  RunStats st;
  dist[static_cast<std::size_t>(s)] = 0;
  pq.push({0, s});
  while (!pq.empty()) {
    st.maxQueue = std::max(st.maxQueue, pq.size());
    const Item top = pq.pop();
    const std::size_t u = static_cast<std::size_t>(top.second);
    if (top.first > dist[u]) continue;
    for (std::size_t k = g.start[u]; k < g.start[u + 1]; ++k) {
      const Weight cand = top.first + g.w[k];
      const std::size_t v = static_cast<std::size_t>(g.to[k]);
      if (cand < dist[v]) {
        dist[v] = cand;
        pq.push({cand, g.to[k]});
      }
    }
  }
  for (Weight d : dist)
    if (d < cbe::kInf) st.checksum += d;
  return st;
}

cbe::Graph randomSparse(int n, std::uint64_t seed) {
  cbe::Rng rng(seed);
  cbe::Graph g(n);
  for (int v = 1; v < n; ++v) g.addEdge(static_cast<int>(rng.below(static_cast<std::uint64_t>(v))), v, rng.range(1, 1000));
  for (int k = 0; k < 2 * n; ++k) {
    const int a = static_cast<int>(rng.below(static_cast<std::uint64_t>(n)));
    const int b = static_cast<int>(rng.below(static_cast<std::uint64_t>(n)));
    if (a != b) g.addEdge(a, b, rng.range(1, 1000));
  }
  return g;
}

cbe::Graph grid(int side, std::uint64_t seed) {
  cbe::Rng rng(seed);
  cbe::Graph g(side * side);
  for (int r = 0; r < side; ++r)
    for (int c = 0; c < side; ++c) {
      if (c + 1 < side) g.addEdge(r * side + c, r * side + c + 1, rng.range(1, 100));
      if (r + 1 < side) g.addEdge(r * side + c, (r + 1) * side + c, rng.range(1, 100));
    }
  return g;
}

// Median wall time of one full single-source run, in microseconds.
template <class PQ>
double medianMicros(const Csr& g, RunStats& out, double budgetSeconds) {
  std::vector<double> times;
  const auto begin = std::chrono::steady_clock::now();
  do {
    const auto t0 = std::chrono::steady_clock::now();
    out = dijkstra<PQ>(g, 0);
    times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
  } while (times.size() < 3 ||
           (std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count() < budgetSeconds &&
            times.size() < 200));
  std::sort(times.begin(), times.end());
  return times[times.size() / 2];
}

}  // namespace

int main(int argc, char** argv) {
  std::map<std::string, std::string> a;
  for (int i = 1; i + 1 < argc; i += 2) a[argv[i]] = argv[i + 1];
  const bool quick = a.count("--quick") && a["--quick"] == "true";
  const int maxV = a.count("--max-v") ? std::atoi(a["--max-v"].c_str()) : 100000;
  const double budget = quick ? 0.02 : 0.3;

  std::vector<int> sizes = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 10000, 25000, 50000, 100000};
  std::printf("%-8s %7s %9s %9s %12s %12s %12s %8s\n", "family", "V", "E", "maxQueue", "heap(us)", "sorted-asc", "sorted-desc",
              "asc/heap");
  bool ok = true;
  for (const char* family : {"sparse", "grid"}) {
    for (int v : sizes) {
      if (v > maxV) continue;
      cbe::Graph graph;
      if (std::string(family) == "sparse") {
        graph = randomSparse(v, 1000 + static_cast<std::uint64_t>(v));
      } else {
        int side = 1;
        while ((side + 1) * (side + 1) <= v) ++side;
        graph = grid(side, 2000 + static_cast<std::uint64_t>(v));
      }
      const Csr c = toCsr(graph);
      RunStats h, sa, sd;
      const double th = medianMicros<HeapPQ>(c, h, budget);
      const double ta = medianMicros<SortedAscPQ>(c, sa, budget);
      const double td = medianMicros<SortedDescPQ>(c, sd, budget);
      if (h.checksum != sa.checksum || h.checksum != sd.checksum) {
        std::printf("CHECKSUM MISMATCH on %s V=%d\n", family, v);
        ok = false;
      }
      if (graph.size() <= 2048) {  // also against the independent reference solver
        const auto ref = cbe::DijkstraSolver().solve(graph, {0})[0];
        Weight sum = 0;
        for (const cbe::Result& r : ref) sum += r.reachable ? r.distance : 0;
        if (sum != h.checksum) {
          std::printf("REFERENCE MISMATCH on %s V=%d\n", family, v);
          ok = false;
        }
      }
      std::printf("%-8s %7d %9zu %9zu %12.1f %12.1f %12.1f %8.2f\n", family, graph.size(), graph.edgeCount(), h.maxQueue, th,
                  ta, td, ta / th);
      std::fflush(stdout);
    }
  }
  return ok ? 0 : 1;
}
