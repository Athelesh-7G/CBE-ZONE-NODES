#include "tt/tinytest.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace tt {

namespace {

Options& optionsStorage() {
  static Options o;
  return o;
}

// Failures recorded by the test that is currently running.
std::vector<std::string>*& activeFailures() {
  static std::vector<std::string>* p = nullptr;
  return p;
}

std::string xmlEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:
        // Control characters other than tab/newline are illegal in XML 1.0.
        if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r')
          out += '?';
        else
          out += c;
    }
  }
  return out;
}

bool globMatch(const std::string& pat, std::size_t p, const std::string& s, std::size_t i) {
  if (p == pat.size()) return i == s.size();
  if (pat[p] == '*') {
    for (std::size_t k = i; k <= s.size(); ++k)
      if (globMatch(pat, p + 1, s, k)) return true;
    return false;
  }
  return i < s.size() && pat[p] == s[i] && globMatch(pat, p + 1, s, i + 1);
}

// A pattern without '*' is a substring match; with '*' it must match the whole name.
bool patternMatches(const std::string& pattern, const std::string& name) {
  if (pattern.find('*') == std::string::npos) return name.find(pattern) != std::string::npos;
  return globMatch(pattern, 0, name, 0);
}

const char* statusText(Status s) {
  switch (s) {
    case Status::Passed: return "PASS";
    case Status::Failed: return "FAIL";
    case Status::Skipped: return "SKIP";
    case Status::Errored: return "ERROR";
  }
  return "?";
}

}  // namespace

std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

Registrar::Registrar(const char* suite, const char* name, std::function<void()> fn) {
  registry().push_back({suite, name, std::move(fn)});
}

const Options& currentOptions() { return optionsStorage(); }
std::uint64_t seed() { return optionsStorage().seed; }
std::string option(const std::string& key, const std::string& fallback) {
  const auto& extra = optionsStorage().extra;
  const auto it = extra.find(key);
  return it == extra.end() ? fallback : it->second;
}

Options parseArgs(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a.rfind("--", 0) != 0) throw std::invalid_argument("unexpected argument: " + a);
    const std::string key = a.substr(2);
    if (key == "list") {
      o.list = true;
      continue;
    }
    if (i + 1 >= argc) throw std::invalid_argument("missing value for " + a);
    const std::string value = argv[++i];
    if (key == "filter") {
      o.filter = value;
    } else if (key == "seed") {
      char* end = nullptr;
      o.seed = std::strtoull(value.c_str(), &end, 10);
      if (end == value.c_str() || *end != '\0')
        throw std::invalid_argument("--seed needs an unsigned integer, got: " + value);
    } else if (key == "junit") {
      o.junitPath = value;
    } else {
      o.extra[key] = value;
    }
  }
  return o;
}

bool matchesFilter(const std::string& fullName, const std::string& filter) {
  if (filter.empty()) return true;
  std::vector<std::string> include, exclude;
  std::istringstream ss(filter);
  std::string part;
  while (std::getline(ss, part, ',')) {
    if (part.empty()) continue;
    if (part[0] == '-')
      exclude.push_back(part.substr(1));
    else
      include.push_back(part);
  }
  for (const std::string& e : exclude)
    if (patternMatches(e, fullName)) return false;
  if (include.empty()) return true;
  for (const std::string& p : include)
    if (patternMatches(p, fullName)) return true;
  return false;
}

namespace detail {

void record(const char* file, int line, const std::string& message, bool fatal) {
  std::ostringstream os;
  os << file << ":" << line << ": " << message;
  if (activeFailures() != nullptr) activeFailures()->push_back(os.str());
  else std::cerr << os.str() << "\n";
  if (fatal) throw FatalFailure{};
}

void skip(const std::string& reason) { throw SkipSignal{reason}; }

}  // namespace detail

RunSummary runCases(const std::vector<TestCase>& cases, const Options& opts, std::ostream& text) {
  const Options saved = optionsStorage();
  optionsStorage() = opts;
  RunSummary summary;
  const auto runStart = std::chrono::steady_clock::now();
  text << "seed: " << opts.seed << "\n";
  for (const TestCase& tc : cases) {
    if (!matchesFilter(tc.fullName(), opts.filter)) continue;
    TestResult r;
    r.suite = tc.suite;
    r.name = tc.name;
    std::vector<std::string> failures;
    std::vector<std::string>* const outer = activeFailures();  // runCases may be nested
    activeFailures() = &failures;
    const auto start = std::chrono::steady_clock::now();
    try {
      tc.fn();
    } catch (const detail::FatalFailure&) {
      // already recorded
    } catch (const detail::SkipSignal& s) {
      r.status = Status::Skipped;
      r.note = s.reason;
    } catch (const std::exception& e) {
      r.status = Status::Errored;
      r.note = std::string("uncaught exception: ") + e.what();
    } catch (...) {
      r.status = Status::Errored;
      r.note = "uncaught non-standard exception";
    }
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    activeFailures() = outer;
    r.failures = failures;
    if (r.status == Status::Passed && !failures.empty()) r.status = Status::Failed;
    text << "[" << std::setw(5) << statusText(r.status) << "] " << tc.fullName() << " ("
         << std::fixed << std::setprecision(1) << r.seconds * 1000 << " ms)\n";
    for (const std::string& f : r.failures) text << "        " << f << "\n";
    if (!r.note.empty()) text << "        " << r.note << "\n";
    summary.results.push_back(std::move(r));
  }
  summary.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count();
  text << summary.results.size() << " tests: " << summary.count(Status::Passed) << " passed, "
       << summary.count(Status::Failed) << " failed, " << summary.count(Status::Errored)
       << " errors, " << summary.count(Status::Skipped) << " skipped in " << std::fixed
       << std::setprecision(3) << summary.seconds << " s\n";
  optionsStorage() = saved;
  return summary;
}

RunSummary runAll(const Options& opts, std::ostream& text) {
  return runCases(registry(), opts, text);
}

void writeJUnit(const RunSummary& s, std::ostream& os, const std::string& suiteName) {
  os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  os << "<testsuite name=\"" << xmlEscape(suiteName) << "\" tests=\"" << s.results.size()
     << "\" failures=\"" << s.count(Status::Failed) << "\" errors=\"" << s.count(Status::Errored)
     << "\" skipped=\"" << s.count(Status::Skipped) << "\" time=\"" << std::fixed
     << std::setprecision(6) << s.seconds << "\">\n";
  for (const TestResult& r : s.results) {
    os << "  <testcase classname=\"" << xmlEscape(r.suite) << "\" name=\"" << xmlEscape(r.name)
       << "\" time=\"" << std::fixed << std::setprecision(6) << r.seconds << "\"";
    if (r.status == Status::Passed) {
      os << "/>\n";
      continue;
    }
    os << ">\n";
    if (r.status == Status::Skipped) {
      os << "    <skipped message=\"" << xmlEscape(r.note) << "\"/>\n";
    } else if (r.status == Status::Errored) {
      os << "    <error message=\"" << xmlEscape(r.note) << "\"/>\n";
    } else {
      std::string all;
      for (const std::string& f : r.failures) all += f + "\n";
      os << "    <failure message=\"" << xmlEscape(r.failures.empty() ? "failed" : r.failures[0])
         << "\">" << xmlEscape(all) << "</failure>\n";
    }
    os << "  </testcase>\n";
  }
  os << "</testsuite>\n";
}

int mainEntry(int argc, char** argv) {
  Options opts;
  try {
    opts = parseArgs(argc, argv);
  } catch (const std::invalid_argument& e) {
    std::cerr << "error: " << e.what() << "\n"
              << "usage: " << argv[0]
              << " [--filter PATTERN] [--seed N] [--junit FILE] [--list] [--key value ...]\n";
    return 2;
  }
  if (opts.list) {
    for (const TestCase& tc : registry())
      if (matchesFilter(tc.fullName(), opts.filter)) std::cout << tc.fullName() << "\n";
    return 0;
  }
  const RunSummary summary = runAll(opts, std::cout);
  if (!opts.junitPath.empty()) {
    std::ofstream out(opts.junitPath);
    if (!out) {
      std::cerr << "error: cannot write " << opts.junitPath << "\n";
      return 2;
    }
    writeJUnit(summary, out, "cbe-verification");
  }
  return summary.ok() ? 0 : 1;
}

}  // namespace tt
