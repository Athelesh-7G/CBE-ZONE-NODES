#include <map>
#include <set>

#include "cbe/generator.hpp"
#include "cbe/rng.hpp"
#include "property_support.hpp"

namespace {

using namespace cbe;

std::uint64_t fingerprint(const Graph& g) {
  std::uint64_t h = 1469598103934665603ULL;  // FNV-1a over n and the edge list
  auto mix = [&h](std::uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
  mix(static_cast<std::uint64_t>(g.size()));
  for (const Edge& e : g.edges()) {
    mix(static_cast<std::uint64_t>(e.u));
    mix(static_cast<std::uint64_t>(e.v));
    mix(static_cast<std::uint64_t>(e.w));
  }
  return h;
}

constexpr std::uint64_t kGoldenFingerprint = 17338477708499429466ULL;

}  // namespace

TT_TEST(Generator, SameSeedGivesSameGraph) {
  for (std::uint64_t seed : {1ULL, 2ULL, 99ULL}) {
    Rng a(seed), b(seed);
    TT_EXPECT_EQ(fingerprint(generateCase(a, 200).graph), fingerprint(generateCase(b, 200).graph));
  }
}

TT_TEST(Generator, GoldenFingerprintIsStableAcrossCompilers) {
  // Recorded on one machine. If this fails on another compiler or standard library,
  // the generator is not portable and "reproduce from a seed" would not hold there.
  Rng rng(deriveSeed(12345, 7));
  const GeneratedCase gc = generateCase(rng, 200);
  std::cout << "    golden case: " << gc.label << " fingerprint " << fingerprint(gc.graph) << "\n";
  TT_EXPECT_EQ(fingerprint(gc.graph), kGoldenFingerprint);
}

TT_TEST(Generator, CoversShapesSizesAndWeightFeatures) {
  std::set<std::string> shapes;
  int minN = 1000, maxN = 0;
  bool zero = false, parallel = false, disconnected = false, huge = false;
  for (std::uint64_t i = 0; i < 600; ++i) {
    Rng rng(deriveSeed(7, i));
    const GeneratedCase gc = generateCase(rng, 200);
    shapes.insert(gc.label.substr(0, gc.label.find('/')));
    minN = std::min(minN, gc.graph.size());
    maxN = std::max(maxN, gc.graph.size());
    std::set<std::pair<int, int>> pairs;
    for (const Edge& e : gc.graph.edges()) {
      zero = zero || e.w == 0;
      huge = huge || e.w > 100000000;
      parallel = parallel || !pairs.insert({std::min(e.u, e.v), std::max(e.u, e.v)}).second;
    }
    // Disconnected: fewer edges than a spanning tree needs, or detected by labels.
    disconnected = disconnected || gc.label.rfind("disconnected", 0) == 0;
  }
  TT_EXPECT_EQ(shapes.size(), 9u);
  TT_EXPECT_LE(minN, 3);
  TT_EXPECT(maxN >= 100);
  TT_EXPECT(maxN <= 200 + 14);  // grids may round up slightly
  TT_EXPECT(zero);
  TT_EXPECT(parallel);
  TT_EXPECT(huge);
  TT_EXPECT(disconnected);
}
