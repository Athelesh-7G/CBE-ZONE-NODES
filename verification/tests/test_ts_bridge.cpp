// Cross-checks the TypeScript implementation (lib/dijkstra.ts) with the C++
// solvers. The TypeScript side is run by tests/cpp/export.ts and run_cases.ts;
// this file only reads the files they wrote (pass --ts-dir <dir>).
#include <fstream>

#include "cbe/differential.hpp"
#include "cbe/external_io.hpp"
#include "cbe/solvers.hpp"
#include "tt/tinytest.hpp"

namespace {

using namespace cbe;

std::string tsDir() { return tt::option("ts-dir"); }

std::ifstream openOrSkip(const std::string& file) {
  const std::string dir = tsDir();
  if (dir.empty()) TT_SKIP("--ts-dir not given (run `npm run test:cpp`)");
  std::ifstream in(dir + "/" + file);
  if (!in) TT_FAIL("cannot open " << dir << "/" << file);
  return in;
}

}  // namespace

TT_TEST(RealGraph, ShapeIs16NodesAnd28Edges) {
  auto in = openOrSkip("graph.txt");
  const auto blocks = parseBlocks(in);
  TT_ASSERT_EQ(blocks.size(), 1u);
  TT_EXPECT_EQ(blocks[0].graph.size(), 16);
  TT_EXPECT_EQ(blocks[0].graph.edgeCount(), 28u);
  TT_EXPECT_EQ(blocks[0].queries.size(), 240u);
}

TT_TEST(RealGraph, ThreeSolversAgreeOnAllPairs) {
  auto in = openOrSkip("graph.txt");
  const Block b = parseBlocks(in).at(0);
  DijkstraSolver dij;
  BellmanFordSolver bf;
  FloydWarshallSolver fw;
  const DiffReport rep = differential(b.graph, {&dij, &bf, &fw}, allSources(b.graph));
  TT_EXPECT_MSG(rep.ok(), rep.summary());
  // 16 sources x 16 targets x 2 solvers compared with the reference.
  TT_EXPECT_EQ(rep.pairsChecked, 512u);
}

TT_TEST(RealGraph, TypeScriptMatchesAll240OrderedPairs) {
  auto gin = openOrSkip("graph.txt");
  const Block b = parseBlocks(gin).at(0);
  auto rin = openOrSkip("ts_results.txt");
  const auto results = parseResults(rin);
  TT_ASSERT(results.count("cbe-real") == 1);
  const std::vector<External>& ts = results.at("cbe-real");
  TT_ASSERT_EQ(ts.size(), 240u);
  // Every ordered pair with s != d appears exactly once.
  std::set<std::pair<int, int>> seen;
  for (const External& e : ts) {
    TT_EXPECT(e.s != e.t);
    seen.insert({e.s, e.t});
  }
  TT_EXPECT_EQ(seen.size(), 240u);
  // The queries the TypeScript side was asked about are the same 240 pairs.
  TT_EXPECT_EQ(b.queries.size(), 240u);

  DijkstraSolver dij;
  BellmanFordSolver bf;
  FloydWarshallSolver fw;
  // TypeScript against each reference solver separately.
  for (const Solver* oracle : std::vector<const Solver*>{&dij, &bf, &fw}) {
    const DiffReport rep = checkExternal(b.graph, "typescript", ts, *oracle);
    TT_EXPECT_MSG(rep.ok(), "vs " << oracle->name() << ": " << rep.summary());
    TT_EXPECT_EQ(rep.pairsChecked, 240u);
    TT_EXPECT_EQ(rep.pathsValidated, 240u);  // the real graph is connected
  }
  std::cout << "    typescript matched 240/240 ordered pairs against 3 solvers\n";
}

TT_TEST(RandomGraphs, TypeScriptMatchesOnGeneratedGraphs) {
  if (tsDir().empty()) TT_SKIP("--ts-dir not given");
  std::ifstream cin_(tsDir() + "/random_cases.txt");
  std::ifstream rin(tsDir() + "/ts_random_results.txt");
  if (!cin_ || !rin) TT_SKIP("random bridge files not present");
  const auto blocks = parseBlocks(cin_);
  const auto results = parseResults(rin);
  TT_ASSERT(!blocks.empty());
  TT_EXPECT_EQ(results.size(), blocks.size());
  DijkstraSolver dij;
  std::size_t queries = 0, bad = 0, unreachable = 0;
  for (const Block& b : blocks) {
    auto it = results.find(b.name);
    if (it == results.end()) TT_FAIL("no TypeScript results for " << b.name);
    TT_ASSERT_EQ(it->second.size(), b.queries.size());
    const DiffReport rep = checkExternal(b.graph, "typescript", it->second, dij);
    queries += rep.pairsChecked;
    for (const External& e : it->second) unreachable += e.found ? 0 : 1;
    if (!rep.ok()) {
      ++bad;
      TT_EXPECT_MSG(false, b.name << ": " << rep.summary());
    }
  }
  std::cout << "    typescript checked on " << blocks.size() << " random graphs, " << queries
            << " queries (" << unreachable << " unreachable), " << bad << " graphs with mismatches\n";
}
