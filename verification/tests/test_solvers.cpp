// Unit tests for the reference solvers, the path validator and the differential
// engine itself (the checkers must be shown to catch bugs, not only to pass).
#include <fstream>
#include <sstream>

#include "cbe/differential.hpp"
#include "cbe/graph.hpp"
#include "cbe/solvers.hpp"
#include "tt/tinytest.hpp"

namespace {

using namespace cbe;

DijkstraSolver dij;
BellmanFordSolver bf;
FloydWarshallSolver fw;
const std::vector<const Solver*> kRefs = {&dij, &bf, &fw};

Graph diamond() {
  // 0-1 (1), 0-2 (4), 1-2 (2), 1-3 (6), 2-3 (3): shortest 0->3 is 0,1,2,3 = 6.
  Graph g(4);
  g.addEdge(0, 1, 1);
  g.addEdge(0, 2, 4);
  g.addEdge(1, 2, 2);
  g.addEdge(1, 3, 6);
  g.addEdge(2, 3, 3);
  return g;
}

}  // namespace

TT_TEST(Solvers, KnownAnswerOnHandMadeGraph) {
  const Graph g = diamond();
  for (const Solver* s : kRefs) {
    const auto r = s->solve(g, {0})[0][3];
    TT_EXPECT_MSG(r.reachable, s->name());
    TT_EXPECT_MSG(r.distance == 6, s->name() << " got " << r.distance);
    TT_EXPECT_MSG((r.path == std::vector<int>{0, 1, 2, 3}), s->name());
  }
}

TT_TEST(Solvers, SourceEqualsTarget) {
  const Graph g = diamond();
  for (const Solver* s : kRefs) {
    const auto r = s->solve(g, {2})[0][2];
    TT_EXPECT(r.reachable);
    TT_EXPECT_EQ(r.distance, 0);
    TT_EXPECT((r.path == std::vector<int>{2}));
  }
}

TT_TEST(Solvers, UnreachableNodes) {
  Graph g(4);
  g.addEdge(0, 1, 5);
  g.addEdge(2, 3, 1);
  for (const Solver* s : kRefs) {
    const auto r = s->solve(g, {0})[0][3];
    TT_EXPECT_MSG(!r.reachable, s->name());
    TT_EXPECT_MSG(r.path.empty(), s->name());
    TT_EXPECT_MSG(r.distance == kInf, s->name());
  }
}

TT_TEST(Solvers, ParallelEdgesAndZeroWeights) {
  Graph g(3);
  g.addEdge(0, 1, 9);
  g.addEdge(0, 1, 2);  // cheaper parallel edge wins
  g.addEdge(1, 2, 0);  // zero-weight edge
  for (const Solver* s : kRefs) {
    const auto r = s->solve(g, {0})[0][2];
    TT_EXPECT_MSG(r.distance == 2, s->name() << " got " << r.distance);
  }
  TT_EXPECT(differential(g, kRefs, allSources(g)).ok());
}

TT_TEST(Solvers, BellmanFordDetectsNegativeCycle) {
  // Directed triangle 0->1->2->0 with total weight -1.
  const std::vector<DirectedEdge> cyc = {{0, 1, 1}, {1, 2, 1}, {2, 0, -3}};
  TT_EXPECT(bellmanFordDirected(3, cyc, 0).negativeCycle);
  // The same triangle with total weight +1 has no negative cycle.
  const std::vector<DirectedEdge> ok = {{0, 1, 1}, {1, 2, 1}, {2, 0, -1}};
  TT_EXPECT(!bellmanFordDirected(3, ok, 0).negativeCycle);
}

TT_TEST(Solvers, BellmanFordHandlesNegativeEdgeWithoutCycle) {
  // 0->1 (4), 0->2 (5), 2->1 (-3): the best way to 1 is 0,2,1 = 2.
  const std::vector<DirectedEdge> e = {{0, 1, 4}, {0, 2, 5}, {2, 1, -3}};
  const auto out = bellmanFordDirected(3, e, 0);
  TT_EXPECT(!out.negativeCycle);
  TT_EXPECT_EQ(out.dist[1], 2);
  TT_EXPECT_EQ(out.pred[1], 2);
}

TT_TEST(Graph, RejectsInvalidEdges) {
  Graph g(3);
  bool threw = false;
  try { g.addEdge(1, 1, 1); } catch (const std::invalid_argument&) { threw = true; }
  TT_EXPECT_MSG(threw, "self-loop accepted");
  threw = false;
  try { g.addEdge(0, 3, 1); } catch (const std::invalid_argument&) { threw = true; }
  TT_EXPECT_MSG(threw, "out-of-range endpoint accepted");
  threw = false;
  try { g.addEdge(0, 1, -1); } catch (const std::invalid_argument&) { threw = true; }
  TT_EXPECT_MSG(threw, "negative weight accepted");
  threw = false;
  try { g.addEdge(0, 1, kMaxWeight + 1); } catch (const std::invalid_argument&) { threw = true; }
  TT_EXPECT_MSG(threw, "oversized weight accepted");
}

TT_TEST(Graph, TextFormatRoundTrips) {
  const Graph g = diamond();
  std::ostringstream os;
  writeBlock(os, "diamond", g, {{0, 3}, {3, 0}});
  std::istringstream is(os.str());
  const auto blocks = parseBlocks(is);
  TT_ASSERT_EQ(blocks.size(), 1u);
  TT_EXPECT_EQ(blocks[0].name, "diamond");
  TT_EXPECT_EQ(blocks[0].graph.size(), 4);
  TT_EXPECT_EQ(blocks[0].graph.edgeCount(), 5u);
  TT_EXPECT_EQ(blocks[0].queries.size(), 2u);
}

TT_TEST(Graph, ParserRejectsMalformedInput) {
  for (const char* bad : {"graph x\n2 1 0\n0 5 1\n", "graph x\n2 2 0\n0 1 1\n",
                          "oops\n", "graph x\n2 1 1\n0 1 1\n0 9\n"}) {
    std::istringstream is(bad);
    bool threw = false;
    try { parseBlocks(is); } catch (const std::runtime_error&) { threw = true; }
    TT_EXPECT_MSG(threw, "accepted: " << bad);
  }
}

// ---- the checkers must catch bugs -----------------------------------------

TT_TEST(Validator, AcceptsRealPathsAndRejectsBadOnes) {
  const Graph g = diamond();
  Result good;
  good.reachable = true;
  good.distance = 6;
  good.path = {0, 1, 2, 3};
  TT_EXPECT_EQ(validatePath(g, good, 0, 3), "");

  Result r = good;
  r.path = {1, 2, 3};
  TT_EXPECT(!validatePath(g, r, 0, 3).empty());  // wrong start
  r = good;
  r.path = {0, 1, 2};
  TT_EXPECT(!validatePath(g, r, 0, 3).empty());  // wrong end
  r = good;
  r.path = {0, 3};
  r.distance = 6;
  TT_EXPECT(!validatePath(g, r, 0, 3).empty());  // 0-3 is not an edge
  r = good;
  r.path = {0, 1, 0, 1, 2, 3};
  TT_EXPECT(!validatePath(g, r, 0, 3).empty());  // repeated node
  r = good;
  r.distance = 7;
  TT_EXPECT(!validatePath(g, r, 0, 3).empty());  // weights do not sum to distance
  r = good;
  r.reachable = false;
  TT_EXPECT(!validatePath(g, r, 0, 3).empty());  // unreachable but carries a path
}

TT_TEST(Differential, ReportsBrokenSolver) {
  const Graph g = diamond();
  // A deliberately wrong solver: correct paths, distances inflated by one.
  FunctionSolver wrong("off-by-one", [](const Graph& gr, int s, int t) {
    Result r = DijkstraSolver().solve(gr, {s})[0][static_cast<std::size_t>(t)];
    if (r.reachable && s != t) r.distance += 1;
    return r;
  });
  const DiffReport rep = differential(g, {&dij, &wrong}, allSources(g));
  TT_EXPECT(!rep.ok());
  bool sawDistance = false;
  for (const Mismatch& m : rep.mismatches)
    if (m.kind == "distance" && m.solver == "off-by-one") sawDistance = true;
  TT_EXPECT_MSG(sawDistance, rep.summary());
}

TT_TEST(Differential, ReportsBrokenPath) {
  const Graph g = diamond();
  FunctionSolver wrong("bad-path", [](const Graph& gr, int s, int t) {
    Result r = DijkstraSolver().solve(gr, {s})[0][static_cast<std::size_t>(t)];
    if (r.reachable && r.path.size() > 2) r.path.erase(r.path.begin() + 1);  // skip a hop
    return r;
  });
  const DiffReport rep = differential(g, {&dij, &wrong}, allSources(g));
  bool sawPath = false;
  for (const Mismatch& m : rep.mismatches)
    if (m.kind == "path") sawPath = true;
  TT_EXPECT_MSG(sawPath, rep.summary());
}

TT_TEST(Differential, AgreesOnHandMadeGraph) {
  const Graph g = diamond();
  const DiffReport rep = differential(g, kRefs, allSources(g));
  TT_EXPECT_MSG(rep.ok(), rep.summary());
  // 4 sources x 4 targets x 2 non-reference solvers.
  TT_EXPECT_EQ(rep.pairsChecked, 32u);
}
