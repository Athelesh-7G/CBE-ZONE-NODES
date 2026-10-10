// Regression corpus: every file in verification/corpus/ is a graph (and queries) that
// once exposed a bug, shrunk by the reducer. Replaying them on every run keeps those
// bugs from coming back. Files are in the normal block format.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "cbe/mutation.hpp"
#include "property_support.hpp"

using namespace cbe;
using namespace cbe_test;

#ifndef CBE_CORPUS_DIR
#error "CBE_CORPUS_DIR must be defined by the build"
#endif

TT_TEST(Corpus, ReplayEveryCorpusFile) {
  namespace fs = std::filesystem;
  const fs::path dir = tt::option("corpus-dir", CBE_CORPUS_DIR);
  if (!fs::exists(dir)) TT_FAIL("corpus directory missing: " << dir.string());
  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(dir))
    if (entry.path().extension() == ".graph") files.push_back(entry.path());
  std::sort(files.begin(), files.end());
  std::size_t queries = 0;
  for (const fs::path& f : files) {
    std::ifstream in(f);
    std::vector<Block> blocks;
    try {
      blocks = parseBlocks(in);
    } catch (const std::exception& e) {
      TT_FAIL(f.filename().string() << ": " << e.what());
    }
    for (const Block& b : blocks) {
      UutSolver uut(16);
      const DiffReport rep = differential(b.graph, {referenceSolvers()[0], referenceSolvers()[1],
                                                    referenceSolvers()[2], &uut},
                                          allSources(b.graph));
      TT_EXPECT_MSG(rep.ok(), f.filename().string() << ": " << rep.summary());
      for (const Query& q : b.queries) {
        ++queries;
        // The recorded query itself, with the early-exit search.
        const Result want = BellmanFordSolver().solve(b.graph, {q.s})[0][ix(q.t)];
        Result got;
        try {
          got = dijkstraUut(b.graph, q.s, q.t, activeMutant());
        } catch (const BudgetExceeded&) {
          TT_EXPECT_MSG(false, f.filename().string() << ": search did not terminate");
          continue;
        }
        TT_EXPECT_MSG(got.reachable == want.reachable && (!got.reachable || got.distance == want.distance) &&
                          validatePath(b.graph, got, q.s, q.t).empty(),
                      f.filename().string() << ": wrong answer for " << q.s << "->" << q.t);
      }
      const UutOutput o = dijkstraUutRaw(b.graph, b.queries.empty() ? 0 : b.queries[0].s, -1, activeMutant());
      std::size_t reachable = 0;
      for (Weight d : o.dist) reachable += d < kInf ? 1 : 0;
      TT_EXPECT_MSG(o.stats.expansions <= reachable, f.filename().string() << ": node expanded twice");
    }
  }
  std::cout << "    replayed " << files.size() << " corpus files, " << queries << " queries\n";
}
