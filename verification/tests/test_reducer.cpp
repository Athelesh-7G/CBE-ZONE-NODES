#include "cbe/mutation.hpp"
#include "property_support.hpp"

using namespace cbe;
using namespace cbe_test;

namespace {

bool hasWeight(const Graph& g, Weight w) {
  for (const Edge& e : g.edges())
    if (e.w == w) return true;
  return false;
}

// Is removing any single edge enough to stop the failure? (1-minimality)
bool oneMinimal(const ReducedCase& r, const FailurePredicate& fails) {
  for (std::size_t skip = 0; skip < r.graph.edgeCount(); ++skip) {
    Graph g(r.graph.size());
    for (std::size_t i = 0; i < r.graph.edgeCount(); ++i)
      if (i != skip) g.addEdge(r.graph.edges()[i].u, r.graph.edges()[i].v, r.graph.edges()[i].w);
    if (fails(g, r.s, r.t)) return false;
  }
  return true;
}

}  // namespace

TT_TEST(Reducer, ShrinksToTheTwoEdgesTheFailureNeeds) {
  // Synthetic failure: it needs an edge of weight 7 AND an edge of weight 3.
  Graph g(30);
  for (int i = 0; i + 1 < 30; ++i) g.addEdge(i, i + 1, 1 + (i % 2) * 8);  // weights 1 and 9
  g.addEdge(4, 20, 7);
  g.addEdge(11, 25, 3);
  const FailurePredicate fails = [](const Graph& gr, int, int) { return hasWeight(gr, 7) && hasWeight(gr, 3); };
  TT_ASSERT(fails(g, 0, 29));
  const ReducedCase r = reduceFailure(g, 0, 29, fails);
  TT_EXPECT(fails(r.graph, r.s, r.t));
  TT_EXPECT_EQ(r.graph.edgeCount(), 2u);
  TT_EXPECT_LE(r.graph.size(), 6);  // s, t and the four touched endpoints only
  TT_EXPECT(oneMinimal(r, fails));
}

TT_TEST(Reducer, ReturnsTheInputWhenNothingCanBeRemoved) {
  Graph g(2);
  g.addEdge(0, 1, 5);
  const FailurePredicate fails = [](const Graph& gr, int, int) { return gr.edgeCount() == 1; };
  const ReducedCase r = reduceFailure(g, 0, 1, fails);
  TT_EXPECT_EQ(r.graph.edgeCount(), 1u);
  TT_EXPECT_EQ(r.graph.size(), 2);
}

TT_TEST(Reducer, ReducesARealMutantFailureToOneMinimalReproducer) {
  // Independent of the active mutant: the mutant is fixed here, so this tests the
  // reducer, not the code under test.
  const Mutant m = Mutant::SkipLastNeighbor;
  // Take the largest failing graph among the first 200, so the reduction is not trivial.
  bool found = false;
  GeneratedCase best{Graph(0), ""};
  Query bestQuery{0, 0};
  for (std::uint64_t i = 0; i < 200; ++i) {
    Rng rng(deriveSeed(tt::seed(), 1000 + i));
    GeneratedCase gc = generateCase(rng, 30);
    const std::optional<Query> q = findFailingQuery(gc.graph, m);
    if (q && (!found || gc.graph.edgeCount() > best.graph.edgeCount())) {
      best = std::move(gc);
      bestQuery = *q;
      found = true;
    }
  }
  if (!found) TT_FAIL("no failing case found for the mutant in 200 generated graphs");
  const FailurePredicate fails = [=](const Graph& gr, int s, int t) { return uutFails(gr, s, t, m); };
  const ReducedCase r = reduceFailure(best.graph, bestQuery.s, bestQuery.t, fails);
  TT_EXPECT(fails(r.graph, r.s, r.t));
  TT_EXPECT(oneMinimal(r, fails));
  TT_EXPECT_LE(r.graph.edgeCount(), best.graph.edgeCount());
  TT_EXPECT_LE(r.graph.edgeCount(), 6u);  // this bug needs very little structure
  std::cout << "    reduced " << best.label << " (" << best.graph.edgeCount() << " edges) to "
            << r.graph.size() << " nodes / " << r.graph.edgeCount() << " edges in "
            << r.predicateCalls << " predicate calls\n";
}
