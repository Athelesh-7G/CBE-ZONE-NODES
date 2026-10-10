// main() for cbe_tests: the generic tt entry point plus `--mutant NAME`, which runs the
// suite against one injected bug (useful to see a failure and its repro line).
#include <cstring>
#include <iostream>

#include "cbe/uut.hpp"
#include "tt/tinytest.hpp"

int main(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--mutant") != 0) continue;
    bool found = false;
    for (const cbe::MutantInfo& m : cbe::allMutants())
      if (std::strcmp(argv[i + 1], m.name) == 0) {
        cbe::activeMutant() = m.id;
        found = true;
      }
    if (!found && std::strcmp(argv[i + 1], "none") != 0) {
      std::cerr << "error: unknown mutant " << argv[i + 1] << "\n";
      return 2;
    }
  }
  return tt::mainEntry(argc, argv);
}
