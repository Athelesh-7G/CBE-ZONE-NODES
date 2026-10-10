// Fuzz target: parse arbitrary bytes as a graph file and run the differential engine.
//
// Fuzzes the C++ verification code itself (parser robustness, solvers agreeing with
// each other, path validation). It does NOT fuzz the TypeScript implementation; that
// is covered by the seeded bridge in tests/cpp/run_cases.ts.
//
// Built two ways (see CMakeLists.txt):
//   * cbe_fuzz         libFuzzer + ASan + UBSan (clang only, -DCBE_FUZZ=ON)
//   * cbe_fuzz_replay  any compiler: runs the same function over files given as
//                      arguments, so seeds and crash files can be replayed anywhere.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "cbe/differential.hpp"
#include "cbe/graph.hpp"
#include "cbe/solvers.hpp"
#include "cbe/uut.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > 4096) return 0;
  std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
  std::vector<cbe::Block> blocks;
  try {
    blocks = cbe::parseBlocks(in);
  } catch (const std::exception&) {
    return 0;  // malformed input is expected; it must be rejected cleanly, not crash
  }
  cbe::DijkstraSolver dij;
  cbe::BellmanFordSolver bf;
  cbe::FloydWarshallSolver fw;
  cbe::UutSolver uut(8);
  for (const cbe::Block& b : blocks) {
    if (b.graph.size() > 64 || b.graph.edgeCount() > 600) continue;  // keep each run fast
    const cbe::DiffReport rep = cbe::differential(b.graph, {&dij, &bf, &fw, &uut}, cbe::allSources(b.graph));
    if (!rep.ok()) {
      std::cerr << "DIFFERENTIAL FAILURE: " << rep.summary() << "\n";
      std::abort();
    }
  }
  return 0;
}
