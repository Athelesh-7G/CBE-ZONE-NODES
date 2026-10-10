// Differential engine: run several solvers on one input, compare their distances,
// validate every returned path, and report disagreements as structured data.
#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cbe/graph.hpp"
#include "cbe/solvers.hpp"

namespace cbe {

struct Mismatch {
  std::string kind;     // "distance", "reachability", "path", "exception"
  std::string solver;   // the solver that disagrees or is invalid
  std::string against;  // the reference it was compared with ("" for path/exception)
  int s = -1;
  int t = -1;
  std::string detail;

  std::string str() const {
    std::ostringstream os;
    os << kind << " [" << solver;
    if (!against.empty()) os << " vs " << against;
    os << "] " << s << "->" << t << ": " << detail;
    return os.str();
  }
};

struct DiffReport {
  std::size_t pairsChecked = 0;     // (source, target) pairs compared
  std::size_t pathsValidated = 0;   // reachable results whose path was validated
  std::vector<Mismatch> mismatches;

  bool ok() const { return mismatches.empty(); }

  void merge(const DiffReport& other) {
    pairsChecked += other.pairsChecked;
    pathsValidated += other.pathsValidated;
    mismatches.insert(mismatches.end(), other.mismatches.begin(), other.mismatches.end());
  }

  std::string summary(std::size_t maxItems = 5) const {
    std::ostringstream os;
    os << mismatches.size() << " mismatch(es) in " << pairsChecked << " pairs";
    for (std::size_t i = 0; i < mismatches.size() && i < maxItems; ++i)
      os << "\n    " << mismatches[i].str();
    if (mismatches.size() > maxItems) os << "\n    ...";
    return os.str();
  }
};

// Returns an empty string when the result is a valid answer for (s, t), otherwise
// the reason it is not. A reachable result must have a path that starts at s, ends
// at t, repeats no node, uses only real edges, and whose edge weights (cheapest
// parallel edge per hop) sum to the reported distance.
inline std::string validatePath(const Graph& g, const Result& r, int s, int t) {
  if (!r.reachable) {
    if (!r.path.empty()) return "unreachable result carries a path";
    return "";
  }
  if (r.path.empty()) return "reachable result has an empty path";
  if (r.path.front() != s) return "path does not start at the source";
  if (r.path.back() != t) return "path does not end at the target";
  std::set<int> seen;
  Weight sum = 0;
  for (std::size_t i = 0; i < r.path.size(); ++i) {
    const int node = r.path[i];
    if (node < 0 || node >= g.size()) return "path node out of range";
    if (!seen.insert(node).second) return "path repeats node " + std::to_string(node);
    if (i + 1 < r.path.size()) {
      const int next = r.path[i + 1];
      if (next < 0 || next >= g.size()) return "path node out of range";
      const Weight hop = g.minEdge(node, next);
      if (hop >= kInf)
        return "no edge " + std::to_string(node) + "-" + std::to_string(next);
      sum += hop;
    }
  }
  if (sum != r.distance)
    return "edge weights sum to " + std::to_string(sum) + " but distance is " +
           std::to_string(r.distance);
  return "";
}

namespace detail {

inline void compareOne(const Graph& g, const std::string& refName, const Result& ref,
                       const std::string& name, const Result& got, int s, int t,
                       DiffReport& report) {
  ++report.pairsChecked;
  if (got.reachable != ref.reachable) {
    report.mismatches.push_back(
        {"reachability", name, refName, s, t,
         std::string(name) + (got.reachable ? " says reachable" : " says unreachable") +
             ", " + refName + " disagrees"});
  } else if (got.reachable && got.distance != ref.distance) {
    report.mismatches.push_back({"distance", name, refName, s, t,
                                 std::to_string(got.distance) + " vs " +
                                     std::to_string(ref.distance)});
  }
  if (got.reachable) ++report.pathsValidated;
  const std::string why = validatePath(g, got, s, t);
  if (!why.empty()) report.mismatches.push_back({"path", name, "", s, t, why});
}

}  // namespace detail

// Runs every solver on `sources` (all targets each). The first solver is the
// reference; all others are compared against it. Every solver's paths are
// validated independently of the comparison.
inline DiffReport differential(const Graph& g, const std::vector<const Solver*>& solvers,
                               const std::vector<int>& sources) {
  DiffReport report;
  std::vector<std::vector<std::vector<Result>>> all;  // [solver][source][target]
  std::vector<bool> usable;
  for (const Solver* sv : solvers) {
    try {
      all.push_back(sv->solve(g, sources));
      usable.push_back(true);
    } catch (const std::exception& e) {
      all.emplace_back();
      usable.push_back(false);
      report.mismatches.push_back({"exception", sv->name(), "", -1, -1, e.what()});
    }
  }
  if (solvers.empty() || !usable[0]) return report;
  for (std::size_t si = 0; si < sources.size(); ++si) {
    for (int t = 0; t < g.size(); ++t) {
      const Result& ref = all[0][si][ix(t)];
      detail::compareOne(g, solvers[0]->name(), ref, solvers[0]->name(), ref, sources[si], t,
                         report);
      --report.pairsChecked;  // the reference is not compared with itself
      for (std::size_t k = 1; k < solvers.size(); ++k) {
        if (!usable[k]) continue;
        detail::compareOne(g, solvers[0]->name(), ref, solvers[k]->name(), all[k][si][ix(t)],
                           sources[si], t, report);
      }
    }
  }
  return report;
}

inline std::vector<int> allSources(const Graph& g) {
  std::vector<int> v(ix(g.size()));
  for (int i = 0; i < g.size(); ++i) v[ix(i)] = i;
  return v;
}

// An answer produced elsewhere (for example by the TypeScript implementation).
struct External {
  int s;
  int t;
  bool found;
  Weight distance;
  std::vector<int> path;
};

// Checks externally produced answers against an oracle solver and validates their
// paths against the graph.
inline DiffReport checkExternal(const Graph& g, const std::string& name,
                                const std::vector<External>& answers, const Solver& oracle) {
  DiffReport report;
  std::set<int> srcSet;
  for (const External& a : answers) {
    if (a.s < 0 || a.t < 0 || a.s >= g.size() || a.t >= g.size()) {
      report.mismatches.push_back({"exception", name, "", a.s, a.t, "query out of range"});
      return report;
    }
    srcSet.insert(a.s);
  }
  const std::vector<int> sources(srcSet.begin(), srcSet.end());
  const auto truth = oracle.solve(g, sources);
  std::map<int, std::size_t> row;
  for (std::size_t i = 0; i < sources.size(); ++i) row[sources[i]] = i;
  for (const External& a : answers) {
    Result got;
    got.reachable = a.found;
    got.distance = a.found ? a.distance : kInf;
    got.path = a.found ? a.path : std::vector<int>{};
    detail::compareOne(g, oracle.name(), truth[row[a.s]][ix(a.t)], name, got, a.s, a.t, report);
  }
  return report;
}

}  // namespace cbe
