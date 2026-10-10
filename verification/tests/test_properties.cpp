// Seeded property tests over generated graphs (sizes 2..200, many shapes).
//
// Properties that need a second implementation (agreement) and properties that do
// not (symmetry, triangle inequality, optimality certificate, monotonicity). The
// second kind judge a solver on its own, so they still work if all references share
// a blind spot.
#include <map>

#include "property_support.hpp"

namespace cbe_test {

namespace {
cbe::DijkstraSolver g_dij;
cbe::BellmanFordSolver g_bf;
cbe::FloydWarshallSolver g_fw;
const std::vector<const cbe::Solver*> g_refs = {&g_dij, &g_bf, &g_fw};
}  // namespace

const std::vector<const cbe::Solver*>& referenceSolvers() { return g_refs; }
const std::vector<const cbe::Solver*>& solversUnderTest() { return g_refs; }

}  // namespace cbe_test

using namespace cbe;
using namespace cbe_test;

namespace {

// Rows of results for `sources`, from one solver.
std::vector<std::vector<Result>> rows(const Solver& s, const Graph& g, const std::vector<int>& src) {
  return s.solve(g, src);
}

Graph withEdgeWeight(const Graph& g, std::size_t index, Weight w) {
  Graph out(g.size());
  for (std::size_t i = 0; i < g.edges().size(); ++i) {
    const Edge& e = g.edges()[i];
    out.addEdge(e.u, e.v, i == index ? w : e.w);
  }
  return out;
}

Graph withoutEdge(const Graph& g, std::size_t index) {
  Graph out(g.size());
  for (std::size_t i = 0; i < g.edges().size(); ++i)
    if (i != index) out.addEdge(g.edges()[i].u, g.edges()[i].v, g.edges()[i].w);
  return out;
}

struct Dsu {
  std::vector<int> p;
  explicit Dsu(int n) : p(static_cast<std::size_t>(n)) {
    for (int i = 0; i < n; ++i) p[static_cast<std::size_t>(i)] = i;
  }
  int find(int x) {
    while (p[static_cast<std::size_t>(x)] != x) {
      p[static_cast<std::size_t>(x)] = p[static_cast<std::size_t>(p[static_cast<std::size_t>(x)])];
      x = p[static_cast<std::size_t>(x)];
    }
    return x;
  }
  void unite(int a, int b) { p[static_cast<std::size_t>(find(a))] = find(b); }
};

}  // namespace

TT_TEST(Properties, AllSolversAgreeAndPathsAreValid) {
  forEachCase("Properties.AllSolversAgreeAndPathsAreValid", 250, [](Ctx& ctx, Rng& rng, const Graph& g) {
    const DiffReport rep = differential(g, solversUnderTest(), pickSources(rng, g));
    PROP_CHECK(ctx, rep.ok(), rep.summary());
  });
}

TT_TEST(Properties, DistanceToSelfIsZero) {
  forEachCase("Properties.DistanceToSelfIsZero", 150, [](Ctx& ctx, Rng& rng, const Graph& g) {
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto r = rows(*sv, g, src);
      for (std::size_t i = 0; i < src.size(); ++i) {
        const Result& self = r[i][static_cast<std::size_t>(src[i])];
        PROP_CHECK(ctx, self.reachable && self.distance == 0 && self.path == std::vector<int>{src[i]},
                   sv->name() << " source " << src[i]);
      }
    }
  });
}

TT_TEST(Properties, DistancesAreSymmetric) {
  forEachCase("Properties.DistancesAreSymmetric", 200, [](Ctx& ctx, Rng& rng, const Graph& g) {
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto r = rows(*sv, g, src);
      for (std::size_t i = 0; i < src.size(); ++i)
        for (std::size_t j = 0; j < src.size(); ++j) {
          const Result& ab = r[i][static_cast<std::size_t>(src[j])];
          const Result& ba = r[j][static_cast<std::size_t>(src[i])];
          PROP_CHECK(ctx, ab.reachable == ba.reachable && (!ab.reachable || ab.distance == ba.distance),
                     sv->name() << " d(" << src[i] << "," << src[j] << ")=" << ab.distance
                                << " d(" << src[j] << "," << src[i] << ")=" << ba.distance);
        }
    }
  });
}

TT_TEST(Properties, TriangleInequality) {
  forEachCase("Properties.TriangleInequality", 200, [](Ctx& ctx, Rng& rng, const Graph& g) {
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto r = rows(*sv, g, src);
      for (std::size_t a = 0; a < src.size(); ++a)
        for (std::size_t b = 0; b < src.size(); ++b) {
          const Result& ab = r[a][static_cast<std::size_t>(src[b])];
          if (!ab.reachable) continue;
          for (int c = 0; c < g.size(); ++c) {
            const Result& bc = r[b][static_cast<std::size_t>(c)];
            if (!bc.reachable) continue;
            const Result& ac = r[a][static_cast<std::size_t>(c)];
            PROP_CHECK(ctx, ac.reachable && ac.distance <= ab.distance + bc.distance,
                       sv->name() << " a=" << src[a] << " b=" << src[b] << " c=" << c);
          }
        }
    }
  });
}

TT_TEST(Properties, ReachabilityMatchesConnectedComponents) {
  // The expected answer comes from a union-find over the edge list, not from any
  // shortest-path code.
  forEachCase("Properties.ReachabilityMatchesConnectedComponents", 250, [](Ctx& ctx, Rng& rng, const Graph& g) {
    Dsu dsu(g.size());
    for (const Edge& e : g.edges()) dsu.unite(e.u, e.v);
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto r = rows(*sv, g, src);
      for (std::size_t i = 0; i < src.size(); ++i)
        for (int t = 0; t < g.size(); ++t) {
          const bool same = dsu.find(src[i]) == dsu.find(t);
          PROP_CHECK(ctx, r[i][static_cast<std::size_t>(t)].reachable == same,
                     sv->name() << " " << src[i] << "->" << t << " expected " << (same ? "reachable" : "unreachable"));
        }
    }
  });
}

TT_TEST(Properties, OptimalityCertificate) {
  // A distance vector d from s is the shortest-path vector if and only if
  //   d[s] = 0;  |d[u] - d[v]| <= w for every edge (feasibility); and every
  //   reachable t != s has an incoming edge that is tight (d[u] + w == d[t]).
  // Checking this needs no reference solver at all.
  forEachCase("Properties.OptimalityCertificate", 250, [](Ctx& ctx, Rng& rng, const Graph& g) {
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto r = rows(*sv, g, src);
      for (std::size_t i = 0; i < src.size(); ++i) {
        const auto& row = r[i];
        for (const Edge& e : g.edges()) {
          const Result& a = row[static_cast<std::size_t>(e.u)];
          const Result& b = row[static_cast<std::size_t>(e.v)];
          PROP_CHECK(ctx, a.reachable == b.reachable, sv->name() << " edge " << e.u << "-" << e.v);
          if (a.reachable && b.reachable)
            PROP_CHECK(ctx, a.distance <= b.distance + e.w && b.distance <= a.distance + e.w,
                       sv->name() << " edge " << e.u << "-" << e.v << " violates feasibility");
        }
        for (int t = 0; t < g.size(); ++t) {
          const Result& rt = row[static_cast<std::size_t>(t)];
          if (!rt.reachable || t == src[i]) continue;
          bool tight = false;
          for (const Arc& arc : g.arcs(t)) {
            const Result& ru = row[static_cast<std::size_t>(arc.to)];
            if (ru.reachable && ru.distance + arc.w == rt.distance) tight = true;
          }
          PROP_CHECK(ctx, tight, sv->name() << " node " << t << " has no tight incoming edge");
        }
      }
    }
  });
}

TT_TEST(Properties, IncreasingAnEdgeWeightNeverDecreasesDistances) {
  forEachCase("Properties.IncreasingAnEdgeWeightNeverDecreasesDistances", 200, [](Ctx& ctx, Rng& rng, const Graph& g) {
    if (g.edgeCount() == 0) return;
    const std::size_t k = static_cast<std::size_t>(rng.below(g.edgeCount()));
    const Weight old = g.edges()[k].w;
    if (old >= kMaxWeight) return;
    const Weight bigger = std::min<Weight>(kMaxWeight, old + 1 + static_cast<Weight>(rng.below(50)));
    const Graph g2 = withEdgeWeight(g, k, bigger);
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto before = rows(*sv, g, src);
      const auto after = rows(*sv, g2, src);
      for (std::size_t i = 0; i < src.size(); ++i)
        for (int t = 0; t < g.size(); ++t) {
          const Result& x = before[i][static_cast<std::size_t>(t)];
          const Result& y = after[i][static_cast<std::size_t>(t)];
          PROP_CHECK(ctx, x.reachable == y.reachable && (!x.reachable || y.distance >= x.distance),
                     sv->name() << " " << src[i] << "->" << t << " edge#" << k << " " << old << "->" << bigger
                                << ": " << x.distance << " became " << y.distance);
        }
    }
  });
}

TT_TEST(Properties, RemovingAnEdgeNeverDecreasesDistances) {
  forEachCase("Properties.RemovingAnEdgeNeverDecreasesDistances", 200, [](Ctx& ctx, Rng& rng, const Graph& g) {
    if (g.edgeCount() == 0) return;
    const std::size_t k = static_cast<std::size_t>(rng.below(g.edgeCount()));
    const Graph g2 = withoutEdge(g, k);
    const auto src = pickSources(rng, g);
    for (const Solver* sv : solversUnderTest()) {
      const auto before = rows(*sv, g, src);
      const auto after = rows(*sv, g2, src);
      for (std::size_t i = 0; i < src.size(); ++i)
        for (int t = 0; t < g.size(); ++t) {
          const Result& x = before[i][static_cast<std::size_t>(t)];
          const Result& y = after[i][static_cast<std::size_t>(t)];
          // Removing an edge can only make a node unreachable, never newly reachable.
          PROP_CHECK(ctx, !(y.reachable && !x.reachable) && (!y.reachable || y.distance >= x.distance),
                     sv->name() << " " << src[i] << "->" << t << " removed edge#" << k << ": "
                                << x.distance << " became " << y.distance);
        }
    }
  });
}

TT_TEST(Properties, TheSamePropertiesDetectAWrongSolver) {
  // Guard against vacuous properties: a solver that is wrong by one must be caught
  // by the optimality certificate on at least one generated graph.
  cbe::FunctionSolver wrong("plus-one", [](const Graph& gr, int s, int t) {
    Result r = DijkstraSolver().solve(gr, {s})[0][static_cast<std::size_t>(t)];
    if (r.reachable && s != t) r.distance += 1;
    return r;
  });
  bool caught = false;
  for (std::size_t i = 0; i < 20 && !caught; ++i) {
    Rng rng(deriveSeed(tt::seed(), i));
    GeneratedCase gc = generateCase(rng, 40);
    const auto src = pickSources(rng, gc.graph);
    const DiffReport rep = differential(gc.graph, {referenceSolvers()[0], &wrong}, src);
    caught = !rep.ok();
  }
  TT_EXPECT(caught);
}
