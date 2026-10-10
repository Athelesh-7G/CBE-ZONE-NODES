# CBE Zone Map — Dijkstra's Algorithm Visualizer

An interactive shortest-path visualizer built on the Coimbatore City Municipal
Corporation zone map. Pick any two of 16 real Coimbatore localities and watch
Dijkstra's algorithm run over the road graph: nodes light up in the order the
algorithm settles them, the shortest route draws itself edge by edge, and every
relaxation step is recorded in a trace table you can read afterwards. Built as an
AI/ML course assignment.

## What It Does

- **Node selection** — choose a source and destination from the 16 mapped
  localities; the map marks them green and red.
- **Animation sequence** — settled nodes turn blue one at a time in visit order,
  then the shortest path draws itself in green from source to destination.
- **Step-by-step trace** — a table with one row per settled node showing its
  final distance and the tentative distance pushed to each neighbour, with rows
  on the final path highlighted.
- **Distance results** — the full route as a chain of nodes, the total distance
  in km, and a per-leg breakdown of each hop.

## Algorithm

`lib/dijkstra.ts` implements Dijkstra's algorithm over an undirected, positively
weighted graph of 16 nodes and 28 edges representing real Coimbatore localities
and the approximate road distances between them. The frontier is a min-priority
queue backed by a binary-insertion sorted array — O(n) insertion, but at this
graph size that beats a real heap's constant factor — with stale queue entries
skipped once a node is settled. Edge weights are integers from 2 to 10. See
[Verification](#verification) for the tests that check the implementation.

Note: the search stops as soon as the destination is settled, so `distances`
for other nodes in a result are not guaranteed final; `totalDistance` and
`path` are. If the destination is unreachable (or an id is unknown), the result
has `found: false`, an empty `path` and `totalDistance: 0`; unreachable nodes
keep a distance of `Infinity`.

## Tech Stack

| Layer | Choice |
| --- | --- |
| Framework | Next.js 14 (App Router) |
| Language | TypeScript |
| Styling | Tailwind CSS |
| Rendering | SVG overlay on the zone map |

## Nodes Covered

| | |
| --- | --- |
| Sulekkai | Town Hall |
| Thudiyalur | Singanallur |
| Saravanampatti | Vedapatti |
| Kalapatti | Ukkadam |
| Gandhipuram | Kurichi |
| Peelamedu | Perur |
| Neelambur | Pattanam |
| RS Puram | Seeranapalayam |

## Running It

```bash
npm install
npm run dev      # http://localhost:3000
npm run build    # production build
npm test         # run the verification suite
```

## Verification

```bash
npm ci
npm test
```

Tests use Node's built-in test runner via `tsx` (`tests/dijkstra.test.ts`). The
Bellman-Ford reference in `tests/reference/bellmanFord.ts` is test-only and
shares no code with `lib/dijkstra.ts`.

| Group | What it checks |
| --- | --- |
| Graph sanity | Exactly 16 nodes and 28 edges; unique ids; no duplicate edges or self-loops; all weights positive; undirected (symmetric lookup); connected |
| Differential | Dijkstra distance equals Bellman-Ford for all 240 ordered (source, destination) pairs; the pair count is asserted to be 240 |
| Path validation | For all 240 pairs: path starts at source, ends at destination, repeats no node, every hop is a real edge, and edge weights sum to the reported distance (tolerance 1e-9) |
| Edge cases | source == destination gives distance 0 and `[source]`; unknown ids give `found: false`; d(s,t) == d(t,s) |
| Properties | Triangle inequality over all 16³ triples; identical results across repeated runs |
| Stale entries | On a synthetic graph that queues a node twice, the result is correct and the node is settled once (no second step) |
| Unreachable | On a synthetic disconnected graph: `found: false`, empty path, `Infinity` distance |

The code exposes no queue hook, so stale-entry skipping is checked through
observable behavior (one `visited`/`steps` entry per node), not by inspecting
the queue. Removing the `settled.has(...)` check in `lib/dijkstra.ts` makes
that test fail.

Output of `npm test` (Node v24.17.0):

```
ℹ tests 17
ℹ suites 7
ℹ pass 17
ℹ fail 0
ℹ duration_ms 107.872583
```

CI (`.github/workflows/ci.yml`) runs `npm ci`, lint, test and build on every
push and pull request.

A second, independent check lives in [verification/](verification/README.md): a C++17
harness with three reference solvers, seeded random-graph and property tests,
mutation testing, a test-case reducer, sanitizer builds and a fuzz target. It checks
`lib/dijkstra.ts` on the real graph (all 240 ordered pairs) and on generated graphs.
Run it with `npm run test:cpp` (needs CMake and a C++17 compiler); `npm run test:all`
runs both suites. See that README for the exact counts and its limits.

## Notes

- Node coordinates are stored as fractions (0–1) of the map image and scaled to
  its rendered size at draw time, so the overlay stays aligned under
  `object-fit: contain` at any window size.
- `public/zone_map.png` is rendered from `Zone Map.pdf`.
- The zone tint rectangles are rough regions for orientation, not real ward
  boundaries.

## License

MIT. See [LICENSE](LICENSE).
