// Tests that judge the unit under test (the mutable Dijkstra). They read the active
// mutant, so the mutation runner can re-run them against every injected bug.
#include <fstream>
#include <iostream>

#include "cbe/mutation.hpp"
#include "property_support.hpp"

using namespace cbe;
using namespace cbe_test;

TT_TEST(Uut, UnmutatedCodeAgreesWithBellmanFordOnGeneratedGraphs) {
  // Independent of the active mutant: guards against a UUT that is wrong even when
  // no bug is injected (which would make every "killed" mutant meaningless).
  forEachCase("Uut.UnmutatedCodeAgreesWithBellmanFordOnGeneratedGraphs", 200, [](Ctx& ctx, Rng& rng, const Graph& g) {
    for (int s : pickSources(rng, g, 12, 4))
      for (int t = 0; t < g.size(); ++t)
        PROP_CHECK(ctx, !uutFails(g, s, t, Mutant::None), "query " << s << "->" << t);
  });
}

TT_TEST(Uut, ExpandsEachNodeAtMostOnce) {
  // Output-equivalent bugs (stale entries expanded again, wrong queue order) are only
  // visible in the work done, so this checks the work: a full search must expand
  // every reachable node exactly once and never more.
  forEachCase("Uut.ExpandsEachNodeAtMostOnce", 200, [](Ctx& ctx, Rng& rng, const Graph& g) {
    for (int s : pickSources(rng, g, 8, 3)) {
      const UutOutput o = dijkstraUutRaw(g, s, -1, activeMutant());
      std::size_t reachable = 0;
      for (Weight d : o.dist) reachable += d < kInf ? 1 : 0;
      PROP_CHECK(ctx, o.stats.expansions <= reachable,
                 "source " << s << ": " << o.stats.expansions << " expansions for " << reachable << " reachable nodes");
    }
  });
}

TT_TEST(Uut, ActiveMutantAgreesOnTheRealGraph) {
  const std::string dir = tt::option("ts-dir");
  if (dir.empty()) TT_SKIP("--ts-dir not given");
  std::ifstream in(dir + "/graph.txt");
  if (!in) TT_FAIL("cannot open graph.txt");
  const Block b = parseBlocks(in).at(0);
  UutSolver uut(16);  // sample every target with the early-exit search
  const DiffReport rep = differential(b.graph, {referenceSolvers()[0], &uut}, allSources(b.graph));
  TT_EXPECT_MSG(rep.ok(), rep.summary());
}
