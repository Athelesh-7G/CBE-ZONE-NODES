// INTERIM minimal test macros for Phase 1. Replaced by the real framework in Phase 2;
// the macro names and semantics used by the test files stay the same.
#pragma once

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace tt {

struct FatalFailure {};
struct SkipTest {};

inline std::vector<std::pair<std::string, void (*)()>>& registry() {
  static std::vector<std::pair<std::string, void (*)()>> r;
  return r;
}
inline std::map<std::string, std::string>& options() {
  static std::map<std::string, std::string> o;
  return o;
}
inline int& failureCount() {
  static int c = 0;
  return c;
}
inline std::string option(const std::string& key, const std::string& def = "") {
  auto it = options().find(key);
  return it == options().end() ? def : it->second;
}
struct Registrar {
  Registrar(const std::string& name, void (*fn)()) { registry().emplace_back(name, fn); }
};
inline void report(const char* file, int line, const std::string& msg) {
  std::cerr << "    " << file << ":" << line << ": " << msg << "\n";
  ++failureCount();
}
template <class T>
std::string str(const T& v) {
  std::ostringstream os;
  os << v;
  return os.str();
}

inline int runMain(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i += 2)
    if (std::string(argv[i]).rfind("--", 0) == 0) options()[std::string(argv[i]).substr(2)] = argv[i + 1];
  int failed = 0, skipped = 0;
  for (auto& [name, fn] : registry()) {
    int before = failureCount();
    try {
      fn();
    } catch (const FatalFailure&) {
    } catch (const SkipTest&) {
      ++skipped;
      std::cout << "[ SKIP ] " << name << "\n";
      continue;
    } catch (const std::exception& e) {
      report("?", 0, std::string("exception: ") + e.what());
    }
    const bool bad = failureCount() != before;
    failed += bad ? 1 : 0;
    std::cout << (bad ? "[ FAIL ] " : "[  OK  ] ") << name << "\n";
  }
  std::cout << registry().size() << " tests, " << failed << " failed, " << skipped << " skipped\n";
  return failed == 0 ? 0 : 1;
}

}  // namespace tt

#define TT_CAT2(a, b) a##b
#define TT_CAT(a, b) TT_CAT2(a, b)
#define TT_TEST(suite, name)                                                         \
  static void TT_CAT(tt_fn_, TT_CAT(suite, name))();                                 \
  static ::tt::Registrar TT_CAT(tt_reg_, TT_CAT(suite, name))(#suite "." #name,      \
                                                              &TT_CAT(tt_fn_, TT_CAT(suite, name))); \
  static void TT_CAT(tt_fn_, TT_CAT(suite, name))()

#define TT_EXPECT_MSG(cond, msg)                                                     \
  do {                                                                               \
    if (!(cond)) {                                                                   \
      std::ostringstream tt_os_;                                                     \
      tt_os_ << #cond << " | " << msg;                                               \
      ::tt::report(__FILE__, __LINE__, tt_os_.str());                                \
    }                                                                                \
  } while (0)
#define TT_EXPECT(cond) TT_EXPECT_MSG(cond, "")
#define TT_ASSERT(cond)                                                              \
  do {                                                                               \
    if (!(cond)) {                                                                   \
      ::tt::report(__FILE__, __LINE__, #cond);                                       \
      throw ::tt::FatalFailure();                                                    \
    }                                                                                \
  } while (0)
#define TT_EXPECT_EQ(a, b) TT_EXPECT_MSG((a) == (b), ::tt::str(a) << " != " << ::tt::str(b))
#define TT_ASSERT_EQ(a, b)                                                           \
  do {                                                                               \
    if (!((a) == (b))) {                                                             \
      ::tt::report(__FILE__, __LINE__, #a " == " #b " | " + ::tt::str(a) + " != " + ::tt::str(b)); \
      throw ::tt::FatalFailure();                                                    \
    }                                                                                \
  } while (0)
#define TT_FAIL(msg)                                                                 \
  do {                                                                               \
    std::ostringstream tt_os_;                                                       \
    tt_os_ << msg;                                                                   \
    ::tt::report(__FILE__, __LINE__, tt_os_.str());                                  \
    throw ::tt::FatalFailure();                                                      \
  } while (0)
#define TT_SKIP(msg)                                                                 \
  do {                                                                               \
    std::cout << "    skip: " << msg << "\n";                                        \
    throw ::tt::SkipTest();                                                          \
  } while (0)
