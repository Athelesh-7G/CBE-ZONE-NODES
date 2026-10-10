// Driver for cbe_fuzz_replay: runs LLVMFuzzerTestOneInput on each file argument.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main(int argc, char** argv) {
  int files = 0;
  for (int i = 1; i < argc; ++i) {
    std::ifstream in(argv[i], std::ios::binary);
    if (!in) {
      std::cerr << "cannot read " << argv[i] << "\n";
      return 2;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    ++files;
  }
  std::cout << "replayed " << files << " inputs without a failure\n";
  return 0;
}
