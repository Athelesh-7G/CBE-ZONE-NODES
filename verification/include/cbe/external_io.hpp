// Reader for result files produced by the TypeScript bridge (tests/cpp/*.ts).
//
//   results <name> <count>
//   s t found distance k n0 n1 ... n(k-1)      (count lines)
#pragma once

#include <istream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cbe/differential.hpp"

namespace cbe {

inline std::map<std::string, std::vector<External>> parseResults(std::istream& in) {
  std::map<std::string, std::vector<External>> out;
  std::string line;
  std::string current;
  long long remaining = 0;
  for (int no = 1; std::getline(in, line); ++no) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ls(line);
    auto fail = [no](const std::string& m) {
      throw std::runtime_error("results file line " + std::to_string(no) + ": " + m);
    };
    if (remaining == 0) {
      std::string kw;
      if (!(ls >> kw >> current >> remaining) || kw != "results" || remaining < 0)
        fail("expected 'results <name> <count>'");
      out[current];  // an empty block is still a block
      continue;
    }
    External e{};
    long long found = 0, k = 0;
    if (!(ls >> e.s >> e.t >> found >> e.distance >> k) || k < 0) fail("malformed result line");
    e.found = found != 0;
    for (long long i = 0; i < k; ++i) {
      int node = 0;
      if (!(ls >> node)) fail("truncated path");
      e.path.push_back(node);
    }
    out[current].push_back(std::move(e));
    --remaining;
  }
  if (remaining != 0) throw std::runtime_error("results file truncated");
  return out;
}

}  // namespace cbe
