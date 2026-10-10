// Bridge, step 2: run lib/dijkstra.ts on generated random graphs.
//
//   npx tsx tests/cpp/run_cases.ts <cases.txt> <results.txt>
//
// cases.txt is produced by `cbe_gen` (same block format as graph.txt). Node ids
// are passed to dijkstra() as the strings "0".."n-1". lib/ is used unmodified.
import { readFileSync, writeFileSync } from "node:fs"
import { dijkstra } from "../../lib/dijkstra"

const [casesPath, outPath] = process.argv.slice(2)
if (!casesPath || !outPath) throw new Error("usage: run_cases.ts <cases.txt> <results.txt>")

const lines = readFileSync(casesPath, "utf8")
  .split("\n")
  .map((l) => l.trim())
  .filter((l) => l !== "" && !l.startsWith("#"))

const out: string[] = []
let i = 0
let cases = 0
let queriesRun = 0
while (i < lines.length) {
  const [kw, name] = lines[i++].split(/\s+/)
  if (kw !== "graph" || !name) throw new Error(`expected 'graph <name>' near ${lines[i - 1]}`)
  const [n, m, q] = lines[i++].split(/\s+/).map(Number)
  const nodes = Array.from({ length: n }, (_, k) => String(k))
  const edges: Array<[string, string, number]> = []
  for (let k = 0; k < m; k++) {
    const [u, v, w] = lines[i++].split(/\s+/).map(Number)
    edges.push([String(u), String(v), w])
  }
  out.push(`results ${name} ${q}`)
  for (let k = 0; k < q; k++) {
    const [s, t] = lines[i++].split(/\s+/).map(Number)
    const r = dijkstra(nodes, edges, String(s), String(t))
    const path = r.path.map(Number)
    out.push(
      [s, t, r.found ? 1 : 0, r.found ? r.totalDistance : 0, path.length, ...path].join(" ")
    )
    queriesRun++
  }
  cases++
}
writeFileSync(outPath, out.join("\n") + "\n")
console.log(`ran ${queriesRun} queries on ${cases} graphs`)
