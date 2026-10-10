// Tests of the test framework itself (tt), run through its programmatic API so the
// deliberately failing cases do not fail this run.
#include <chrono>
#include <sstream>
#include <thread>

#include "tt/tinytest.hpp"

namespace {

std::vector<std::string>& journal() {
  static std::vector<std::string> j;
  return j;
}

struct LoggingFixture : tt::Fixture {
  void SetUp() override { journal().push_back("setup"); }
  void TearDown() override { journal().push_back("teardown"); }
};

struct FixtureThatFails : LoggingFixture {
  void body() {
    journal().push_back("body");
    TT_ASSERT(false);
    journal().push_back("not reached");
  }
};

tt::RunSummary runOne(std::function<void()> fn, const tt::Options& o = {}) {
  std::ostringstream sink;
  return tt::runCases({{"S", "t", std::move(fn)}}, o, sink);
}

}  // namespace

TT_TEST(Framework, FilterMatching) {
  using tt::matchesFilter;
  TT_EXPECT(matchesFilter("Suite.Name", ""));
  TT_EXPECT(matchesFilter("Suite.Name", "Suite"));        // substring
  TT_EXPECT(matchesFilter("Suite.Name", "ite.Na"));
  TT_EXPECT(!matchesFilter("Suite.Name", "Other"));
  TT_EXPECT(matchesFilter("Suite.Name", "Suite.*"));      // glob, whole name
  TT_EXPECT(!matchesFilter("Suite.Name", "Nam*"));        // glob must match whole name
  TT_EXPECT(matchesFilter("Suite.Name", "Other,Suite.N*"));  // comma list = OR
  TT_EXPECT(!matchesFilter("Suite.Name", "Suite,-Name")); // leading '-' excludes
  TT_EXPECT(matchesFilter("Suite.Other", "Suite,-Name"));
}

TT_TEST(Framework, PassFailSkipAndErrorAreClassified) {
  std::ostringstream sink;
  const std::vector<tt::TestCase> cases = {
      {"S", "pass", [] { TT_EXPECT(true); }},
      {"S", "expect_continues", [] { TT_EXPECT(false); TT_EXPECT_EQ(1, 2); }},
      {"S", "assert_stops", [] { TT_ASSERT(false); TT_EXPECT(false); }},
      {"S", "skip", [] { TT_SKIP("not today"); }},
      {"S", "throws", [] { throw std::runtime_error("boom"); }},
  };
  const tt::RunSummary s = tt::runCases(cases, {}, sink);
  TT_ASSERT_EQ(s.results.size(), 5u);
  TT_EXPECT(s.results[0].status == tt::Status::Passed);
  TT_EXPECT(s.results[1].status == tt::Status::Failed);
  TT_EXPECT_EQ(s.results[1].failures.size(), 2u);  // EXPECT does not stop the test
  TT_EXPECT(s.results[2].status == tt::Status::Failed);
  TT_EXPECT_EQ(s.results[2].failures.size(), 1u);  // ASSERT does
  TT_EXPECT(s.results[3].status == tt::Status::Skipped);
  TT_EXPECT_EQ(s.results[3].note, "not today");
  TT_EXPECT(s.results[4].status == tt::Status::Errored);
  TT_EXPECT(!s.ok());
  TT_EXPECT(runOne([] {}).ok());
  TT_EXPECT(runOne([] { TT_SKIP("x"); }).ok());  // a skip is not a failure
}

TT_TEST(Framework, FailureMessagesCarryFileAndLine) {
  const tt::RunSummary s = runOne([] { TT_EXPECT_EQ(2 + 2, 5); });
  TT_ASSERT_EQ(s.results[0].failures.size(), 1u);
  const std::string& f = s.results[0].failures[0];
  TT_EXPECT(f.find("test_framework.cpp:") != std::string::npos);
  TT_EXPECT(f.find("2 + 2 == 5") != std::string::npos);
  TT_EXPECT(f.find("4 vs 5") != std::string::npos);
}

TT_TEST(Framework, FixtureRunsSetUpAndAlwaysTearsDown) {
  journal().clear();
  const tt::RunSummary s = runOne([] { tt::detail::runFixture<FixtureThatFails>(); });
  TT_EXPECT(s.results[0].status == tt::Status::Failed);
  const std::vector<std::string> expected = {"setup", "body", "teardown"};
  TT_EXPECT(journal() == expected);
}

TT_TEST(Framework, ExpectThrowMacro) {
  TT_EXPECT_THROW(throw std::invalid_argument("x"), std::invalid_argument);
  const tt::RunSummary s = runOne([] { TT_EXPECT_THROW((void)0, std::invalid_argument); });
  TT_EXPECT(s.results[0].status == tt::Status::Failed);
}

TT_TEST(Framework, SeedAndOptionsReachTests) {
  tt::Options o;
  o.seed = 4242;
  o.extra["color"] = "green";
  std::uint64_t seen = 0;
  std::string color;
  runOne([&] { seen = tt::seed(); color = tt::option("color"); }, o);
  TT_EXPECT_EQ(seen, 4242u);
  TT_EXPECT_EQ(color, "green");
  TT_EXPECT_EQ(tt::option("missing", "dflt"), "dflt");
}

TT_TEST(Framework, ParseArgs) {
  const char* argv[] = {"prog", "--filter", "A.*", "--seed", "77", "--junit", "out.xml", "--ts-dir", "d", "--list"};
  const tt::Options o = tt::parseArgs(10, const_cast<char**>(argv));
  TT_EXPECT_EQ(o.filter, "A.*");
  TT_EXPECT_EQ(o.seed, 77u);
  TT_EXPECT_EQ(o.junitPath, "out.xml");
  TT_EXPECT_EQ(o.extra.at("ts-dir"), "d");
  TT_EXPECT(o.list);
  const char* bad[] = {"prog", "--seed", "abc"};
  TT_EXPECT_THROW(tt::parseArgs(3, const_cast<char**>(bad)), std::invalid_argument);
  const char* missing[] = {"prog", "--filter"};
  TT_EXPECT_THROW(tt::parseArgs(2, const_cast<char**>(missing)), std::invalid_argument);
  const char* stray[] = {"prog", "oops"};
  TT_EXPECT_THROW(tt::parseArgs(2, const_cast<char**>(stray)), std::invalid_argument);
}

TT_TEST(Framework, TimingIsMeasuredPerTest) {
  const tt::RunSummary s = runOne([] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
  TT_EXPECT(s.results[0].seconds >= 0.015);
  TT_EXPECT(s.seconds >= s.results[0].seconds);
}

TT_TEST(Framework, JUnitXmlIsWellFormedAndEscaped) {
  std::ostringstream sink;
  const std::vector<tt::TestCase> cases = {
      {"S", "ok", [] {}},
      {"S", "bad", [] { TT_EXPECT_MSG(false, "a<b && c>\"d\""); }},
      {"S", "skipped", [] { TT_SKIP("later"); }},
  };
  const tt::RunSummary s = tt::runCases(cases, {}, sink);
  std::ostringstream xml;
  tt::writeJUnit(s, xml, "suite&name");
  const std::string x = xml.str();
  TT_EXPECT(x.find("<testsuite name=\"suite&amp;name\" tests=\"3\" failures=\"1\" errors=\"0\" skipped=\"1\"") !=
            std::string::npos);
  TT_EXPECT(x.find("<testcase classname=\"S\" name=\"ok\"") != std::string::npos);
  TT_EXPECT(x.find("<failure message=") != std::string::npos);
  TT_EXPECT(x.find("&lt;b &amp;&amp; c&gt;&quot;d&quot;") != std::string::npos);
  TT_EXPECT(x.find("<skipped message=\"later\"/>") != std::string::npos);
  TT_EXPECT(x.find("a<b") == std::string::npos);  // raw '<' from the message must not leak
}
