// tt: a small test framework (header + tinytest.cpp, C++17, standard library only).
//
// Design choices, briefly:
//  * Registration uses static registrars created by macros. The macro is the only
//    way to get a unique name and capture __FILE__/__LINE__, so it is the right use
//    of the preprocessor here. Everything else (runner, reporters, matching) is
//    ordinary C++ in tinytest.cpp.
//  * EXPECT records a failure and continues; ASSERT records and aborts the test by
//    throwing. Throwing is used so destructors (RAII fixtures) still run.
//  * A test is a std::function, so fixtures, plain functions and generated cases all
//    register the same way, and the mutation runner can re-run the whole registry
//    in-process.
//  * Randomness is not owned by the framework: it only carries the --seed value
//    (tt::seed()) and prints it, so a failing run can be repeated exactly.
//
// Usage:
//   TT_TEST(Suite, Name) { TT_EXPECT_EQ(1 + 1, 2); }
//   struct Fx : tt::Fixture { void SetUp() override {...} void TearDown() override {...} };
//   TT_TEST_F(Fx, Name) { ... uses members of Fx ... }
//
// Command line (see tt::parseArgs): --filter PATTERN  --seed N  --junit FILE  --list
//   Any other "--key value" pair is stored and read with tt::option("key").
#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace tt {

struct Options {
  std::string filter;       // comma-separated; '*' wildcard; leading '-' excludes
  std::uint64_t seed = 1;   // master seed for randomized tests
  std::string junitPath;    // write JUnit XML here when non-empty
  bool list = false;        // only print test names
  std::map<std::string, std::string> extra;  // any other --key value pair
};

struct TestCase {
  std::string suite;
  std::string name;
  std::function<void()> fn;
  std::string fullName() const { return suite + "." + name; }
};

enum class Status { Passed, Failed, Skipped, Errored };

struct TestResult {
  std::string suite;
  std::string name;
  Status status = Status::Passed;
  double seconds = 0;
  std::vector<std::string> failures;  // "file:line: message"
  std::string note;                   // skip reason or exception text
};

struct RunSummary {
  std::vector<TestResult> results;
  double seconds = 0;
  std::size_t count(Status s) const {
    std::size_t n = 0;
    for (const TestResult& r : results) n += r.status == s ? 1 : 0;
    return n;
  }
  // Skipped tests do not fail a run.
  bool ok() const { return count(Status::Failed) == 0 && count(Status::Errored) == 0; }
};

// ---- registry --------------------------------------------------------------
std::vector<TestCase>& registry();

struct Registrar {
  Registrar(const char* suite, const char* name, std::function<void()> fn);
};

// ---- options ---------------------------------------------------------------
Options parseArgs(int argc, char** argv);  // throws std::invalid_argument on bad input
const Options& currentOptions();
std::uint64_t seed();
std::string option(const std::string& key, const std::string& fallback = "");

// ---- running and reporting -------------------------------------------------
bool matchesFilter(const std::string& fullName, const std::string& filter);
RunSummary runCases(const std::vector<TestCase>& cases, const Options& opts, std::ostream& text);
RunSummary runAll(const Options& opts, std::ostream& text);  // runs registry()
void writeJUnit(const RunSummary& summary, std::ostream& os, const std::string& suiteName);
int mainEntry(int argc, char** argv);  // the whole main(): returns the exit code

// ---- fixtures --------------------------------------------------------------
class Fixture {
 public:
  virtual ~Fixture() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

namespace detail {

struct FatalFailure {};
struct SkipSignal {
  std::string reason;
};

void record(const char* file, int line, const std::string& message, bool fatal);
[[noreturn]] void skip(const std::string& reason);

template <class T, class = void>
struct Streamable : std::false_type {};
template <class T>
struct Streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <class T>
std::string str(const T& v) {
  if constexpr (Streamable<T>::value) {
    std::ostringstream os;
    os << v;
    return os.str();
  } else {
    return "<unprintable>";
  }
}

inline void failOp(const char* file, int line, const char* expr, const std::string& a,
                   const std::string& b, bool fatal) {
  record(file, line, std::string(expr) + "   [" + a + " vs " + b + "]", fatal);
}

template <class F>
void runFixture() {
  F f;
  f.SetUp();
  struct Guard {
    F& f;
    ~Guard() {
      try {
        f.TearDown();
      } catch (...) {  // a throwing TearDown must not terminate the process
        record("<teardown>", 0, "TearDown threw an exception", false);
      }
    }
  } guard{f};
  f.body();
}

}  // namespace detail
}  // namespace tt

// ---- macros ----------------------------------------------------------------
#define TT_TEST(suite, name)                                                          \
  static void tt_##suite##_##name##_body();                                           \
  static ::tt::Registrar tt_##suite##_##name##_reg(#suite, #name,                     \
                                                   &tt_##suite##_##name##_body);      \
  static void tt_##suite##_##name##_body()

#define TT_TEST_F(fixture, name)                                                      \
  class tt_##fixture##_##name##_Case : public fixture {                               \
   public:                                                                            \
    void body();                                                                      \
  };                                                                                  \
  static ::tt::Registrar tt_##fixture##_##name##_reg(                                 \
      #fixture, #name, [] { ::tt::detail::runFixture<tt_##fixture##_##name##_Case>(); }); \
  void tt_##fixture##_##name##_Case::body()

#define TT_MSG_(cond_text, msg, fatal)                                                \
  do {                                                                                \
    std::ostringstream tt_os_;                                                        \
    tt_os_ << cond_text << msg;                                                       \
    ::tt::detail::record(__FILE__, __LINE__, tt_os_.str(), fatal);                    \
  } while (0)

#define TT_EXPECT_MSG(cond, msg)                                                      \
  do {                                                                                \
    if (!(cond)) TT_MSG_(#cond << " | ", msg, false);                                 \
  } while (0)
#define TT_ASSERT_MSG(cond, msg)                                                      \
  do {                                                                                \
    if (!(cond)) TT_MSG_(#cond << " | ", msg, true);                                  \
  } while (0)
#define TT_EXPECT(cond) TT_EXPECT_MSG(cond, "")
#define TT_ASSERT(cond) TT_ASSERT_MSG(cond, "")

#define TT_OP_(op, a, b, fatal)                                                       \
  do {                                                                                \
    const auto& tt_a_ = (a);                                                          \
    const auto& tt_b_ = (b);                                                          \
    if (!(tt_a_ op tt_b_))                                                            \
      ::tt::detail::failOp(__FILE__, __LINE__, #a " " #op " " #b,                     \
                           ::tt::detail::str(tt_a_), ::tt::detail::str(tt_b_), fatal); \
  } while (0)
#define TT_EXPECT_EQ(a, b) TT_OP_(==, a, b, false)
#define TT_EXPECT_NE(a, b) TT_OP_(!=, a, b, false)
#define TT_EXPECT_LE(a, b) TT_OP_(<=, a, b, false)
#define TT_EXPECT_LT(a, b) TT_OP_(<, a, b, false)
#define TT_ASSERT_EQ(a, b) TT_OP_(==, a, b, true)

#define TT_EXPECT_THROW(stmt, exception_type)                                         \
  do {                                                                                \
    bool tt_threw_ = false;                                                           \
    try {                                                                             \
      stmt;                                                                           \
    } catch (const exception_type&) {                                                 \
      tt_threw_ = true;                                                               \
    } catch (...) {                                                                   \
    }                                                                                 \
    if (!tt_threw_) TT_MSG_("expected " #exception_type " from: " #stmt, "", false);  \
  } while (0)

#define TT_FAIL(msg) TT_MSG_("", msg, true)
#define TT_SKIP(msg)                                                                  \
  do {                                                                                \
    std::ostringstream tt_os_;                                                        \
    tt_os_ << msg;                                                                    \
    ::tt::detail::skip(tt_os_.str());                                                 \
  } while (0)
