// cbe_gen: writes seeded random graph cases (with queries) for the TypeScript bridge.
//
//   cbe_gen --seed S --count N --max-n M --queries Q --out cases.txt
//
// Case i uses deriveSeed(S, i), so any single case can be regenerated alone.
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "cbe/generator.hpp"
#include "cbe/graph.hpp"
#include "cbe/rng.hpp"

int main(int argc, char** argv) {
  std::map<std::string, std::string> a;
  for (int i = 1; i + 1 < argc; i += 2) a[argv[i]] = argv[i + 1];
  const auto get = [&](const char* k, long long d) {
    auto it = a.find(k);
    return it == a.end() ? d : std::atoll(it->second.c_str());
  };
  const std::uint64_t seed = static_cast<std::uint64_t>(get("--seed", 1));
  const long long count = get("--count", 100);
  const int maxN = static_cast<int>(get("--max-n", 60));
  const int nq = static_cast<int>(get("--queries", 30));
  const std::string out = a.count("--out") ? a["--out"] : "cases.txt";

  std::ofstream os(out);
  if (!os) {
    std::cerr << "cannot write " << out << "\n";
    return 2;
  }
  os << "# cbe_gen --seed " << seed << " --count " << count << " --max-n " << maxN << "\n";
  for (long long i = 0; i < count; ++i) {
    cbe::Rng rng(cbe::deriveSeed(seed, static_cast<std::uint64_t>(i)));
    const cbe::GeneratedCase gc = cbe::generateCase(rng, maxN);
    const cbe::Graph& g = gc.graph;
    std::vector<cbe::Query> qs;
    for (int k = 0; k < nq; ++k)
      qs.push_back({static_cast<int>(rng.below(static_cast<std::uint64_t>(g.size()))),
                    static_cast<int>(rng.below(static_cast<std::uint64_t>(g.size())))});
    os << "# " << gc.label << "\n";
    cbe::writeBlock(os, "case-" + std::to_string(i), g, qs);
  }
  std::cout << "wrote " << count << " cases (seed " << seed << ") to " << out << "\n";
  return 0;
}
