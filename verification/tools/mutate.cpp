// cbe_mutate: runs the whole test suite against every injected bug and reports which
// mutants were killed (some test failed) and which survived.
//
//   cbe_mutate [--seed N] [--cases N] [--ts-dir DIR] [--mutant NAME] [--junit FILE]
//
// Mutation score = killed / total mutants. Mutants documented as equivalent (same
// observable behavior as the original) stay in the denominator; they are listed
// separately so the number is not inflated by excluding hard cases.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "cbe/uut.hpp"
#include "tt/tinytest.hpp"

int main(int argc, char** argv) {
  tt::Options opts;
  try {
    opts = tt::parseArgs(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
  // A mutant is dead as soon as one test fails, so stop there (standard practice).
  // `--run-all true` keeps running every test and counts all failing tests.
  opts.failFast = !(opts.extra.count("run-all") && opts.extra.at("run-all") == "true");
  const std::string wanted = opts.extra.count("mutant") ? opts.extra.at("mutant") : "";

  // Suite output (including std::cout lines printed by the tests) is discarded.
  std::ostringstream sinkText;
  std::ostringstream sinkCout;
  std::streambuf* realCout = std::cout.rdbuf();

  auto runSuite = [&](cbe::Mutant m) {
    cbe::activeMutant() = m;
    std::cout.rdbuf(sinkCout.rdbuf());
    sinkCout.str("");
    sinkText.str("");
    const tt::RunSummary s = tt::runAll(opts, sinkText);
    std::cout.rdbuf(realCout);
    cbe::activeMutant() = cbe::Mutant::None;
    return s;
  };

  std::cout << "seed: " << opts.seed << "\n";
  const bool wasFailFast = opts.failFast;
  opts.failFast = false;  // the baseline must run everything
  const auto baseStart = std::chrono::steady_clock::now();
  const tt::RunSummary base = runSuite(cbe::Mutant::None);
  const double baseSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - baseStart).count();
  opts.failFast = wasFailFast;
  std::cout << "baseline (no mutant): " << base.results.size() << " tests, "
            << base.count(tt::Status::Passed) << " passed, " << base.count(tt::Status::Skipped)
            << " skipped, " << base.count(tt::Status::Failed) + base.count(tt::Status::Errored) << " failed, "
            << std::fixed << std::setprecision(2) << baseSeconds << " s\n";
  if (!base.ok()) {
    for (const tt::TestResult& r : base.results)
      if (r.status == tt::Status::Failed || r.status == tt::Status::Errored)
        std::cout << "  baseline failure: " << r.suite << "." << r.name << "\n";
    std::cout << "refusing to report a mutation score for a failing baseline\n";
    return 3;
  }

  struct Row {
    const cbe::MutantInfo* info;
    bool killed;
    std::size_t failingTests;
    std::string firstKiller;
    double seconds;
  };
  std::vector<Row> rows;
  for (const cbe::MutantInfo& info : cbe::allMutants()) {
    if (!wanted.empty() && wanted != info.name) continue;
    const auto t0 = std::chrono::steady_clock::now();
    const tt::RunSummary s = runSuite(info.id);
    Row row{&info, !s.ok(), s.count(tt::Status::Failed) + s.count(tt::Status::Errored), "", 0};
    for (const tt::TestResult& r : s.results)
      if (r.status == tt::Status::Failed || r.status == tt::Status::Errored) {
        row.firstKiller = r.suite + "." + r.name;
        break;
      }
    row.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    rows.push_back(row);
  }

  std::cout << "\n" << std::left << std::setw(26) << "mutant" << std::setw(10) << "result" << std::setw(8)
            << "failing" << "first test that failed" << (opts.failFast ? " (fail-fast)" : "") << "\n";
  std::size_t killed = 0, undocumented = 0;
  for (const Row& r : rows) {
    std::cout << std::setw(26) << r.info->name << std::setw(10) << (r.killed ? "KILLED" : "SURVIVED")
              << std::setw(8) << r.failingTests << (r.killed ? r.firstKiller : "-") << "\n";
    killed += r.killed ? 1 : 0;
    if (!r.killed && std::string(r.info->equivalentNote).empty()) ++undocumented;
  }
  const double score = rows.empty() ? 0.0 : 100.0 * static_cast<double>(killed) / static_cast<double>(rows.size());
  std::cout << "\nmutation score: " << killed << "/" << rows.size() << " killed = " << std::fixed
            << std::setprecision(1) << score << "%\n";
  for (const Row& r : rows)
    if (!r.killed)
      std::cout << "survivor: " << r.info->name << " (" << r.info->description << ") - "
                << (std::string(r.info->equivalentNote).empty() ? "NOT documented as equivalent" : r.info->equivalentNote)
                << "\n";
  if (!opts.junitPath.empty()) {
    std::ofstream out(opts.junitPath);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite name=\"cbe-mutation\" tests=\"" << rows.size()
        << "\" failures=\"" << rows.size() - killed << "\">\n";
    for (const Row& r : rows) {
      out << "  <testcase classname=\"mutant\" name=\"" << r.info->name << "\" time=\"" << r.seconds << "\"";
      if (r.killed) out << "/>\n";
      else out << "><failure message=\"mutant survived\"/></testcase>\n";
    }
    out << "</testsuite>\n";
  }
  return undocumented == 0 ? 0 : 1;
}
