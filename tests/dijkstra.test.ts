import { describe, it } from "node:test"
import assert from "node:assert/strict"
import { dijkstra, edgeWeight } from "../lib/dijkstra"
import { NODES, EDGES } from "../lib/graph"
import { bellmanFord } from "./reference/bellmanFord"

const IDS = NODES.map((n) => n.id)
const EPS = 1e-9
const key = (a: string, b: string) => [a, b].sort().join("|")

const PAIRS: Array<[string, string]> = []
for (const s of IDS) for (const d of IDS) if (s !== d) PAIRS.push([s, d])

describe("graph sanity", () => {
  it("has exactly 16 nodes and 28 edges", () => {
    assert.equal(NODES.length, 16)
    assert.equal(EDGES.length, 28)
  })

  it("has unique node ids", () => {
    assert.equal(new Set(IDS).size, IDS.length)
  })

  it("has no duplicate edges (either direction) and no self-loops", () => {
    const seen = new Set<string>()
    for (const [a, b] of EDGES) {
      assert.notEqual(a, b, `self-loop on ${a}`)
      const k = key(a, b)
      assert.ok(!seen.has(k), `duplicate edge ${k}`)
      seen.add(k)
    }
  })

  it("references only known nodes and has positive finite weights", () => {
    const ids = new Set(IDS)
    for (const [a, b, w] of EDGES) {
      assert.ok(ids.has(a) && ids.has(b), `unknown endpoint in ${a}-${b}`)
      assert.ok(Number.isFinite(w) && w > 0, `bad weight on ${a}-${b}`)
    }
  })

  it("is undirected: each edge is stored once and resolves symmetrically", () => {
    for (const [a, b, w] of EDGES) {
      assert.equal(edgeWeight(EDGES, a, b), w)
      assert.equal(edgeWeight(EDGES, b, a), w)
    }
  })

  it("is connected", () => {
    const adj = new Map<string, string[]>(IDS.map((id) => [id, []]))
    for (const [a, b] of EDGES) {
      adj.get(a)!.push(b)
      adj.get(b)!.push(a)
    }
    const seen = new Set([IDS[0]])
    const stack = [IDS[0]]
    while (stack.length) {
      for (const next of adj.get(stack.pop()!)!) {
        if (!seen.has(next)) {
          seen.add(next)
          stack.push(next)
        }
      }
    }
    assert.equal(seen.size, IDS.length)
  })
})

describe("differential test vs Bellman-Ford", () => {
  it("checks exactly 240 ordered pairs", () => {
    assert.equal(PAIRS.length, 240)
  })

  it("matches Bellman-Ford distance for all 240 ordered pairs", () => {
    const reference = new Map(IDS.map((s) => [s, bellmanFord(IDS, EDGES, s)]))
    let matched = 0
    for (const [s, d] of PAIRS) {
      const result = dijkstra(IDS, EDGES, s, d)
      assert.ok(result.found, `${s}->${d} not found`)
      const expected = reference.get(s)![d]
      assert.ok(
        Math.abs(result.totalDistance - expected) < EPS,
        `${s}->${d}: dijkstra ${result.totalDistance} vs bellman-ford ${expected}`
      )
      matched++
    }
    assert.equal(matched, 240)
  })
})

describe("path validation", () => {
  it("returns a valid path whose edge weights sum to the distance, for all 240 pairs", () => {
    let validated = 0
    for (const [s, d] of PAIRS) {
      const { path, totalDistance, found } = dijkstra(IDS, EDGES, s, d)
      assert.ok(found)
      assert.equal(path[0], s)
      assert.equal(path[path.length - 1], d)
      assert.equal(new Set(path).size, path.length, `${s}->${d} repeats a node`)

      let sum = 0
      for (let i = 0; i < path.length - 1; i++) {
        const hop = EDGES.find(
          ([a, b]) =>
            (a === path[i] && b === path[i + 1]) ||
            (b === path[i] && a === path[i + 1])
        )
        assert.ok(hop, `${s}->${d}: ${path[i]}-${path[i + 1]} is not an edge`)
        sum += hop[2]
      }
      assert.ok(Math.abs(sum - totalDistance) < EPS, `${s}->${d}: sum ${sum} vs ${totalDistance}`)
      validated++
    }
    assert.equal(validated, 240)
  })
})

describe("edge cases", () => {
  it("source == destination gives distance 0 and path [source]", () => {
    for (const id of IDS) {
      const r = dijkstra(IDS, EDGES, id, id)
      assert.equal(r.found, true)
      assert.equal(r.totalDistance, 0)
      assert.deepEqual(r.path, [id])
    }
  })

  it("returns found=false with empty path for unknown node ids", () => {
    for (const [s, d] of [
      ["nowhere", "sulekkai"],
      ["sulekkai", "nowhere"],
      ["", ""],
    ]) {
      const r = dijkstra(IDS, EDGES, s, d)
      assert.equal(r.found, false)
      assert.deepEqual(r.path, [])
      assert.equal(r.totalDistance, 0)
    }
  })

  it("is symmetric: d(s,t) == d(t,s)", () => {
    for (const [s, t] of PAIRS) {
      const a = dijkstra(IDS, EDGES, s, t).totalDistance
      const b = dijkstra(IDS, EDGES, t, s).totalDistance
      assert.ok(Math.abs(a - b) < EPS, `${s}<->${t}: ${a} vs ${b}`)
    }
  })
})

describe("properties", () => {
  it("satisfies the triangle inequality for all triples", () => {
    const D: Record<string, Record<string, number>> = {}
    for (const s of IDS) {
      D[s] = {}
      for (const t of IDS) D[s][t] = dijkstra(IDS, EDGES, s, t).totalDistance
    }
    let triples = 0
    for (const a of IDS)
      for (const b of IDS)
        for (const c of IDS) {
          assert.ok(D[a][c] <= D[a][b] + D[b][c] + EPS, `${a},${b},${c}`)
          triples++
        }
    assert.equal(triples, 16 ** 3)
  })

  it("is deterministic across repeated runs", () => {
    for (const [s, d] of PAIRS) {
      const first = dijkstra(IDS, EDGES, s, d)
      for (let i = 0; i < 3; i++) assert.deepEqual(dijkstra(IDS, EDGES, s, d), first)
    }
  })
})

describe("stale-entry skipping", () => {
  // dijkstra.ts exposes no queue hook, so this checks observable behavior on a
  // graph that forces duplicate pushes: C is pushed at 10 (via A), then again
  // at 3 (via B). The 10 entry is stale and must not cause a second expansion.
  const nodes = ["S", "A", "B", "C", "T"]
  const edges: Array<[string, string, number]> = [
    ["S", "A", 1],
    ["S", "B", 2],
    ["A", "C", 9],
    ["B", "C", 1],
    ["C", "T", 1],
  ]

  it("produces the correct result when a node is queued twice", () => {
    const r = dijkstra(nodes, edges, "S", "T")
    assert.equal(r.totalDistance, 4)
    assert.deepEqual(r.path, ["S", "B", "C", "T"])
  })

  it("settles each node at most once (stale entry not re-expanded)", () => {
    // Run to an unreachable-by-early-exit destination so the queue drains fully.
    const r = dijkstra([...nodes, "Z"], edges, "S", "Z")
    assert.equal(r.found, false)
    assert.equal(new Set(r.visited).size, r.visited.length)
    assert.deepEqual([...r.visited].sort(), [...nodes].sort())
    assert.equal(r.steps.length, nodes.length)
    // C's first (stale, 10) entry would have produced a second step for C.
    assert.equal(r.steps.filter((s) => s.current === "C").length, 1)
    assert.equal(r.distances.C, 3)
  })
})

describe("unreachable destination", () => {
  // Documented behavior: found=false, path=[], totalDistance=0, and the
  // unreachable node's entry in `distances` stays Infinity. It never appears
  // in `visited`.
  const nodes = ["A", "B", "C", "D"]
  const edges: Array<[string, string, number]> = [
    ["A", "B", 1],
    ["C", "D", 1],
  ]

  it("reports found=false with an empty path and Infinity distance", () => {
    const r = dijkstra(nodes, edges, "A", "D")
    assert.equal(r.found, false)
    assert.deepEqual(r.path, [])
    assert.equal(r.totalDistance, 0)
    assert.equal(r.distances.D, Infinity)
    assert.equal(r.distances.C, Infinity)
    assert.deepEqual(r.visited, ["A", "B"])
  })
})
