# C++ verification toolchain

A C++17 test and verification toolchain that checks this project's TypeScript
Dijkstra (`lib/dijkstra.ts`) from the outside. It uses techniques from compiler and
systems verification: independent reference solvers (oracles), differential testing,
seeded property-based testing, mutation testing, delta-debugging test reduction, a
regression corpus, sanitizers, and a fuzz target.

What it is: a verification harness for a shortest-path implementation.
What it is not: a compiler test suite. It does not test a compiler.

Standard library only for the core (no JSON or test libraries). Data moves between
TypeScript and C++ through plain text files.

## Architecture

```text
  lib/dijkstra.ts  (unchanged)                    verification/ (C++17)
        |
        |  tests/cpp/export.ts       graph.txt, ts_results.txt
        +--------------------------------------+
        |  tests/cpp/run_cases.ts  <-- random_cases.txt <-- cbe_gen (seeded)
        |        |                                   |
        |        +--> ts_random_results.txt ---------+
        v                                           v
  +--------------------------- cbe_tests (framework: tt) -----------------------+
  |                                                                              |
  |  Graph/Result     Reference solvers        Differential engine               |
  |  text I/O         Dijkstra (heap)          compare distances                 |
  |  (graph.hpp)      Bellman-Ford             validate every path               |
  |                   Floyd-Warshall           structured Mismatch list          |
  |                                                                              |
  |  Generators (9 shapes)   Property tests          UUT = mutable Dijkstra      |
  |  seeded, portable        8 properties            12 injectable bugs          |
  |                                                                              |
  |  Corpus replay  <---------- cbe_reduce (ddmin) <------ failing graph         |
  +------------------------------------------------------------------------------+
        |                                  |                     |
   cbe_mutate                         cbe_fuzz /            cbe_bench
   suite x every mutant               cbe_fuzz_replay       queue comparison
```

Directory layout:

```text
verification/
  include/cbe/   graph, solvers, differential, generator, rng, uut, reducer, mutation
  framework/tt/  the test framework (tinytest.hpp/.cpp, main.cpp)
  tests/         test_*.cpp (12 suites) and property_support.hpp
  tools/         gen.cpp, mutate.cpp, reduce.cpp
  bench/         bench_pq.cpp
  fuzz/          fuzz_diff.cpp, replay_main.cpp, seeds/
  corpus/        reduced regression graphs (*.graph), replayed on every run
  scripts/       test_cpp.sh (the whole flow)
  CMakeLists.txt, CMakePresets.json
```

## Build and run

Requirements: CMake 3.16+, a C++17 compiler (g++ or clang++), Node 18+ with the
repo's npm dependencies installed (`npm ci`, which provides `tsx`).

```bash
npm run test:cpp      # export from TypeScript, build, run suite, mutation, fuzz replay
npm run test:all      # existing TypeScript tests, then the C++ suite
```

`test:cpp` runs `verification/scripts/test_cpp.sh`, which:

1. exports the real graph and the TypeScript answers for it (`tests/cpp/export.ts`),
2. builds with CMake (`-Wall -Wextra -Wpedantic -Werror`),
3. generates seeded random graphs (`cbe_gen`) and answers them with the unmodified
   `lib/dijkstra.ts` (`tests/cpp/run_cases.ts`),
4. runs `cbe_tests`, then `cbe_mutate`, then replays the fuzz seeds.

Environment variables for the script:

| Variable | Meaning | Default |
| --- | --- | --- |
| `CBE_SEED` | master seed for everything randomized | 20261010 |
| `CBE_BUILD_DIR` | CMake build directory | verification/build/release |
| `CBE_SANITIZE` | `ON` builds with ASan + UBSan | OFF |
| `CBE_MUTATION` | `0` skips the mutation run | 1 |
| `CBE_TS_CASES` | random graphs sent through TypeScript | 150 |
| `CBE_JUNIT` | JUnit XML output path | verification/build/junit.xml |

Other builds:

```bash
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan   # sanitizers
cmake --preset fuzz && cmake --build --preset fuzz                # libFuzzer, clang only
cd verification && ./build/fuzz/cbe_fuzz -max_total_time=60 fuzz/seeds corpus
./build/release/cbe_bench                                         # queue benchmark
```

`cbe_tests` flags: `--mutant NAME` (run against one injected bug), `--filter PATTERN` (comma list; `*` wildcard; `-NAME` excludes),
`--seed N`, `--junit FILE`, `--list`, `--fail-fast`, and `--cases N`, `--max-n N`,
`--case K`, `--ts-dir DIR` (extra `--key value` options read with `tt::option`).
Exit code is 0 when no test failed or errored (skips do not fail a run), 1 otherwise,
2 on a usage error.

## Reproducing a failure from a seed

Every randomized run prints `seed: N`. Each random case derives its own seed from
(master seed, test name, case index), and a failing property prints a repro line:

```text
[repro: --filter Properties.DistanceToSelfIsZero --seed 20261010 --case 0 --mutant wrong-initial-distance ; chain/zeros+parallel/n=144/m=174]
```

(That line is real output from `cbe_tests --mutant wrong-initial-distance`, which runs the
suite against one injected bug.) Run exactly that case:

```bash
verification/build/release/cbe_tests --filter Properties.DistanceToSelfIsZero \
    --seed 20261010 --case 0 --mutant wrong-initial-distance
```

The generators do not use `std::uniform_int_distribution` (its output differs between
standard libraries); they derive values from the raw `std::mt19937_64` output, which is
fully specified. A golden-fingerprint test (`Generator.GoldenFingerprintIsStable...`)
checks that the same seed gives the same graph. It gave the same fingerprint on Apple
clang with libc++ and on GCC 14 with libstdc++.

## What is checked

Differential testing (`include/cbe/differential.hpp`): all solvers run on the same
graph; distances and reachability are compared with the first solver, and every
returned path is validated (starts at the source, ends at the target, no repeated node,
only real edges, edge weights sum to the distance).

Three reference solvers share no logic: Dijkstra with `std::priority_queue` and
lazy deletion, Bellman-Ford with V-1 passes and a negative-cycle detecting extra pass,
and Floyd-Warshall with a next-hop matrix.

Property tests (`tests/test_properties.cpp`), over generated graphs of 2 to 200 nodes
(random, sparse, disconnected, chain, cycle, star, complete, grid, layered; five weight
regimes including ties, zero weights, parallel edges and weights up to 1e9):

| Property | Needs a reference solver? |
| --- | --- |
| all solvers agree and all paths are valid | yes |
| d(s,s) = 0 and the path is `[s]` | no |
| d(s,t) = d(t,s) | no |
| triangle inequality | no |
| reachability equals connected components (union-find) | no |
| optimality certificate: feasible on every edge and tight into every node | no |
| raising one edge weight never decreases a distance | no |
| removing one edge never decreases a distance | no |

The optimality certificate proves a distance vector is the shortest-path vector without
comparing against any other solver.

TypeScript bridge: the real graph is checked on all 240 ordered pairs against all three
solvers; 150 generated graphs (4500 queries) are run through the unmodified
`lib/dijkstra.ts` and compared with the Dijkstra reference. `dijkstra()` accepts an
arbitrary node and edge list, so no change to `lib/` was needed.

## Mutation testing

`include/cbe/uut.hpp` is a C++ Dijkstra in the style of `lib/dijkstra.ts` (lazy
deletion, early exit at the target) with a runtime `Mutant` enum selecting one injected
bug. `cbe_mutate` runs the whole suite against each mutant in the same process and
stops at the first failing test (`--run-all true` disables that).

```text
mutation score = mutants killed / mutants injected      (a mutant is killed when at
                                                         least one test fails or errors)
```

The baseline (no mutant) must pass first; otherwise no score is printed. A mutant that
loops forever is stopped by a work budget and counts as a failure.

| Mutant | Bug | First test that failed |
| --- | --- | --- |
| relax-le | relax with `<=` instead of `<` | Properties.AllSolversAgree... |
| skip-last-neighbor | last adjacency entry never relaxed | Properties.AllSolversAgree... |
| stale-check-off-by-one | stale test `d > dist[u] + 1` | Uut.ExpandsEachNodeAtMostOnce |
| one-direction-only | adjacency built u to v only | Properties.AllSolversAgree... |
| wrong-initial-distance | `dist[source] = 1` | Properties.AllSolversAgree... |
| wrong-predecessor | grandparent stored as predecessor | Properties.AllSolversAgree... |
| early-exit-too-early | stop when target first reached | Properties.AllSolversAgree... |
| int32-overflow | distances wrap at 2^32 | Properties.AllSolversAgree... |
| no-stale-skip | stale queue entries expanded | Uut.ExpandsEachNodeAtMostOnce |
| wrong-heap-order | max-heap instead of min-heap | Properties.AllSolversAgree... |
| zero-weight-skip | zero-weight edges ignored | Properties.AllSolversAgree... |
| keep-first-parallel-edge | first parallel edge, not cheapest | Properties.AllSolversAgree... |

Result of the run with seed 20261010: 12 of 12 killed, score 100.0%, no survivors.

Two of the twelve (`stale-check-off-by-one`, `no-stale-skip`) do not change any output:
a stale entry expanded again relaxes nothing new. I ran them with the work-counting test
and the corpus replay excluded (`--filter "-Uut.ExpandsEachNodeAtMostOnce,-Corpus"`) and
both SURVIVED. That is why `Uut.ExpandsEachNodeAtMostOnce` exists: it checks the work
done (each reachable node is expanded once at most), which the outputs cannot show.
A 100% score here means the suite was written against this list of mutants; it does
not mean there are no other bugs it would miss.

Mutants that survive are listed with a note in the `MutantInfo` table
(`equivalentNote`); `cbe_mutate` exits non-zero if a survivor has no documented reason.

## Test-case reduction and the corpus

`include/cbe/reducer.hpp` implements delta debugging (ddmin) for a failing
(graph, source, target): remove chunks of edges, then single edges (so the result is
1-minimal for edges), drop untouched nodes and renumber, then lower weights toward 0/1
while the failure remains. `cbe_reduce` finds a failing graph for a mutant, reduces it
and writes it to `corpus/`.

Real output on one mutant:

```text
mutant early-exit-too-early fails on case 1 (complete/ties(1..3)/n=25/m=300) query 0->2
reduced 25 nodes / 300 edges  ->  4 nodes / 4 edges (61 predicate calls)
```

The reproducer (`corpus/early-exit-too-early-s1-c1.graph`):

```text
graph early-exit-too-early
4 4 1
0 2 1
0 3 1
1 2 3
1 3 1
0 1
```

(Query `0->1`: the route 0,2,1 costs 4 and the true shortest, 0,3,1, costs 2. Node 2 is
expanded first, and the mutant stops as soon as node 1 gets any distance, so it returns 4.)

`corpus/` holds 10 reduced reproducers (one per output-visible mutant). `Corpus.Replay...`
replays every file on every run against the references and the unit under test. Run alone
(`--filter Corpus`), the corpus kills all 12 mutants.

## Sanitizers, fuzzing, benchmark

- `asan-ubsan` preset: AddressSanitizer + UndefinedBehaviorSanitizer with
  `-fno-sanitize-recover=undefined`, so undefined behavior aborts the test run.
- `cbe_fuzz` (libFuzzer, clang only): parses arbitrary bytes as a graph file and runs the
  differential engine on it. It fuzzes the C++ verification code (parser and solvers),
  not the TypeScript. `cbe_fuzz_replay` runs the same entry point over files with any
  compiler and is part of `npm run test:cpp`.
- `cbe_bench`: a sorted-array priority queue against `std::priority_queue` inside the
  same Dijkstra (see results below).

## Results from real runs

Environment: Apple M5, macOS. Compilers used:

| Compiler | Where | Used for |
| --- | --- | --- |
| Apple clang 21.0.0 (clang-2100.1.1.101), also installed as `g++` | macOS arm64 | main development, sanitizers, benchmark |
| GCC 14.4.0 | Docker image `gcc:14`, Linux aarch64 | full suite and mutation run |
| Debian clang 19.1.7 | Docker, Linux aarch64 | ASan + UBSan full suite, libFuzzer |

Flags: `-std=c++17 -Wall -Wextra -Wpedantic -Werror`; Release builds add `-O3 -DNDEBUG`;
sanitizer builds add `-fsanitize=address,undefined -fno-sanitize-recover=undefined
-fno-omit-frame-pointer -g`.

Suite (`cbe_tests`, seed 20261010): 45 tests in 12 suites, 45 passed, 4.4 s
(Apple clang, Release). Identical result with GCC 14.4.0 (4.7 s) and with ASan + UBSan
(Apple clang 23.7 s; Linux clang 19 Debug 178.8 s).

Random generated cases executed by the property and UUT tests: 2100. TypeScript bridge:
240 of 240 ordered pairs on the real graph matched, and 150 random graphs / 4500 queries
(224 unreachable) had 0 mismatches. The whole `npm run test:all` takes about 21 s on this
machine.

Fuzzing: libFuzzer on Debian clang 19, 51,961 executions in 46 s starting from the seeds
and corpus, no failure.

### Benchmark: sorted array vs binary heap (C++, this machine)

Dijkstra from node 0, median of repeated runs, microseconds. `sorted-asc` keeps the queue
ascending and pops the front (what `lib/dijkstra.ts` does); `sorted-desc` keeps it
descending and pops the back. `asc/heap` is the ratio. The three queues returned identical
distance checksums, and for V <= 2048 the checksum also matched the reference solver.

```text
family         V         E  maxQueue     heap(us)   sorted-asc  sorted-desc asc/heap
sparse        16        46        16          0.7          0.9          0.6     1.31
sparse        32        95        36          1.2          2.4          1.5     2.07
sparse        64       190        53          2.0          5.6          3.0     2.87
sparse       128       379       100          4.2         19.5          9.0     4.63
sparse       256       767       225         10.8         55.2         24.8     5.12
sparse       512      1533       428         15.5        135.4         53.2     8.74
sparse      1024      3069       857         23.8        373.4        163.6    15.67
sparse      2048      6139      1744         53.0       1409.8        571.0    26.62
sparse      4096     12286      3442        231.2       5376.5       2118.9    23.25
sparse     10000     29995      8422        923.1      32116.0      12057.5    34.79
sparse     25000     74997     21108       2837.8     224951.5      78220.2    79.27
sparse     50000    149997     41918       6365.4     922165.7     320464.1   144.87
sparse    100000    299998     83987      14022.0    3680448.8    1270723.7   262.48
grid          16        24         6          0.2          0.2          0.2     1.25
grid          25        40         8          0.2          0.3          0.3     1.33
grid          64       112        16          0.6          1.0          0.8     1.78
grid         121       220        30          1.3          2.5          1.7     1.93
grid         256       480        42          3.0          5.6          3.6     1.86
grid         484       924        54          5.8         12.4          8.4     2.13
grid        1024      1984        91         12.8         35.6         25.3     2.77
grid        2025      3960       134         28.1        118.1         77.7     4.20
grid        4096      8064       179         82.1        322.7        207.7     3.93
grid       10000     19800       284        409.2       1124.5        689.1     2.75
grid       24964     49612       444       1350.9       4091.4       2465.4     3.03
grid       49729     99012       622       3021.9      10662.2       6214.6     3.53
grid       99856    199080       885       6698.4      28539.2      16253.2     4.26
```

Reading it: in C++ the binary heap is as fast as or faster than the ascending sorted array
at every size measured (V = 16: 0.7 us vs 0.9 us on the sparse graph, 0.2 us vs 0.2 us on
the grid), and the gap grows steadily with V. There is no
crossover in this range. At V = 16, `sorted-desc` and the heap are within what I would
treat as noise (0.6 vs 0.7 us; 0.2 vs 0.2 us on the grid), and the whole run is under a
microsecond either way. So the code comment in `lib/dijkstra.ts` ("the constant factor
beats a real heap here") is not supported by this benchmark for C++.

This does not settle the TypeScript case: V8's `Array.prototype.splice` and `shift` are
not C++'s `vector::insert` and `erase`. I also ran a quick, uncommitted scratch comparison
in Node (copy of `lib/dijkstra.ts` with the queue swapped for a binary heap, same
distances): all 240 pairs of the real graph took a median 0.772 ms with the sorted array
and 0.725 ms with the heap (200 repetitions), so they are close at this size and the heap
was not slower; on random sparse graphs of 64 to 2048 nodes the heap was also faster
(for example 13.5 ms vs 11.8 ms for 20 queries at V = 1024). Treat that as indicative
only; it is not part of the repo.

## Limits

- The real graph is fixed (16 nodes, 28 edges). Randomized graphs are generated, which
  samples a large space; it is not exhaustive. The 240-pair check is exhaustive only for
  that one graph.
- The oracles are mine. Three independent solvers and the optimality certificate reduce
  the chance of a shared mistake; they do not remove it.
- Mutation score is relative to a list of mutants I wrote. 12/12 does not mean the
  suite finds all bugs.
- The fuzz target fuzzes the C++ harness, not `lib/dijkstra.ts`. TypeScript is exercised
  through the seeded bridge only.
- Graph model: undirected, integer weights 0 to 1e9, no self-loops. Path sums are
  assumed far below `kInf`; graphs near 2.3e9 nodes are out of scope.
- The framework `tt` is small on purpose (about 490 lines): no parallel execution, no
  output capture, no death tests.
- This checks a shortest-path implementation. It is not a compiler test suite.

## The `tt` framework, design choices

- Static registrars created by macros: the macro is the only way to get a unique name
  plus `__FILE__`/`__LINE__`; everything else (matching, running, reporting) is plain C++.
- A test is a `std::function`, so plain tests, fixtures and generated cases register the
  same way, and `cbe_mutate` can re-run the whole registry in-process.
- `EXPECT` records and continues; `ASSERT` records and throws. Exceptions are used so
  RAII fixtures (`TearDown`) still run.
- Fixtures use `SetUp`/`TearDown` with a guard object, so teardown runs after a fatal
  failure and a throwing teardown cannot terminate the process.
- The framework only carries the seed. Randomness lives in `cbe/rng.hpp` so that tests,
  tools and the generator agree on how values are derived.
- Reports: human-readable text with per-test timing, and JUnit XML for CI. Skipped tests
  do not fail a run.
- The test objects are linked into each executable that needs them (an OBJECT library),
  because a static library would drop unreferenced static registrars.
