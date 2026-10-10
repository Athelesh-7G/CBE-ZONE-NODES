// cbe_reduce: finds a graph on which an injected bug is visible, shrinks it with
// delta debugging, and writes the minimal reproducer to the regression corpus.
//
//   cbe_reduce --mutant NAME [--seed N] [--max-cases N] [--max-n N] [--out-dir DIR]
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

#include "cbe/generator.hpp"
#include "cbe/mutation.hpp"
#include "cbe/rng.hpp"

int main(int argc, char** argv) {
  std::map<std::string, std::string> a;
  for (int i = 1; i + 1 < argc; i += 2) a[argv[i]] = argv[i + 1];
  if (!a.count("--mutant")) {
    std::cerr << "usage: cbe_reduce --mutant NAME [--seed N] [--max-cases N] [--max-n N] [--out-dir DIR]\n";
    for (const cbe::MutantInfo& m : cbe::allMutants()) std::cerr << "  " << m.name << "\n";
    return 2;
  }
  const cbe::MutantInfo* info = nullptr;
  for (const cbe::MutantInfo& m : cbe::allMutants())
    if (a["--mutant"] == m.name) info = &m;
  if (!info) {
    std::cerr << "unknown mutant " << a["--mutant"] << "\n";
    return 2;
  }
  const std::uint64_t seed = a.count("--seed") ? std::stoull(a["--seed"]) : 1;
  const std::size_t maxCases = a.count("--max-cases") ? std::stoul(a["--max-cases"]) : 500;
  const int maxN = a.count("--max-n") ? std::stoi(a["--max-n"]) : 40;
  const std::string outDir = a.count("--out-dir") ? a["--out-dir"] : "corpus";

  for (std::size_t i = 0; i < maxCases; ++i) {
    cbe::Rng rng(cbe::deriveSeed(seed, i));
    const cbe::GeneratedCase gc = cbe::generateCase(rng, maxN);
    const auto q = cbe::findFailingQuery(gc.graph, info->id);
    if (!q) continue;
    std::cout << "mutant " << info->name << " fails on case " << i << " (" << gc.label << ") query " << q->s
              << "->" << q->t << "\n";
    const cbe::Mutant m = info->id;
    const cbe::FailurePredicate fails = [m](const cbe::Graph& g, int s, int t) { return cbe::uutFails(g, s, t, m); };
    const cbe::ReducedCase r = cbe::reduceFailure(gc.graph, q->s, q->t, fails);
    std::cout << "reduced " << gc.graph.size() << " nodes / " << gc.graph.edgeCount() << " edges  ->  "
              << r.graph.size() << " nodes / " << r.graph.edgeCount() << " edges (" << r.predicateCalls
              << " predicate calls)\n";
    std::filesystem::create_directories(outDir);
    const std::string file = outDir + "/" + info->name + "-s" + std::to_string(seed) + "-c" + std::to_string(i) + ".graph";
    std::ofstream os(file);
    os << "# regression case reduced by cbe_reduce\n# mutant: " << info->name << " (" << info->description << ")\n"
       << "# found with --seed " << seed << " case " << i << " (" << gc.label << ")\n"
       << "# the correct implementation must answer query " << r.s << "->" << r.t << " correctly\n";
    cbe::writeBlock(os, std::string(info->name), r.graph, {{r.s, r.t}});
    std::cout << "wrote " << file << "\n";
    for (const cbe::Edge& e : r.graph.edges()) std::cout << "  " << e.u << " - " << e.v << "  w=" << e.w << "\n";
    return 0;
  }
  std::cout << "mutant " << info->name << ": no output-visible failure in " << maxCases
            << " cases (it can only be caught by work/statistics checks, if at all)\n";
  return 1;
}
