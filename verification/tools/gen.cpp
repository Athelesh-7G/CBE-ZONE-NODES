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
    cbe::GenParams p;
    p.n = static_cast<int>(rng.range(2, maxN));
    p.density = 0.02 + 0.4 * rng.unit();
    p.connected = rng.chance(0.6);
    p.maxWeight = rng.chance(0.3) ? 1'000'000 : 30;
    p.pZeroWeight = rng.chance(0.3) ? 0.15 : 0.0;
    p.pParallel = rng.chance(0.3) ? 0.2 : 0.0;
    const cbe::Graph g = cbe::randomGraph(rng, p);
    std::vector<cbe::Query> qs;
    for (int k = 0; k < nq; ++k)
      qs.push_back({static_cast<int>(rng.below(static_cast<std::uint64_t>(p.n))),
                    static_cast<int>(rng.below(static_cast<std::uint64_t>(p.n)))});
    cbe::writeBlock(os, "case-" + std::to_string(i), g, qs);
  }
  std::cout << "wrote " << count << " cases (seed " << seed << ") to " << out << "\n";
  return 0;
}
