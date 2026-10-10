#!/usr/bin/env bash
# Runs the whole C++ verification flow: export from TypeScript, build, generate
# random cases, run them through the TypeScript implementation, run the C++ suite.
#
#   CBE_SEED=123 npm run test:cpp        reproduce a run with a given seed
#   CBE_BUILD_DIR=... CBE_SANITIZE=ON    choose build directory / sanitizer build
set -euo pipefail
cd "$(dirname "$0")/../.."

SEED="${CBE_SEED:-20261010}"
BUILD="${CBE_BUILD_DIR:-verification/build/release}"
OUT="${CBE_TS_DIR:-verification/build/ts}"
JUNIT="${CBE_JUNIT:-verification/build/junit.xml}"
CASES="${CBE_TS_CASES:-150}"

mkdir -p "$OUT"
echo "== export real graph from TypeScript"
npx tsx tests/cpp/export.ts "$OUT"

echo "== build C++ ($BUILD)"
cmake -S verification -B "$BUILD" -DCBE_SANITIZE="${CBE_SANITIZE:-OFF}" >/dev/null
cmake --build "$BUILD" -j

echo "== generate random cases (seed $SEED) and run them through lib/dijkstra.ts"
"$BUILD/cbe_gen" --seed "$SEED" --count "$CASES" --max-n 60 --queries 30 --out "$OUT/random_cases.txt"
npx tsx tests/cpp/run_cases.ts "$OUT/random_cases.txt" "$OUT/ts_random_results.txt"

echo "== run C++ suite"
"$BUILD/cbe_tests" --ts-dir "$OUT" --seed "$SEED" --junit "$JUNIT"
