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
skipped once a node is settled. The implementation was verified against a
Bellman-Ford reference across all 240 source/destination pairs: every distance
matches, and every returned path reconnects through real edges and sums to its
reported total.

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
```

## Notes

- Node coordinates are stored as fractions (0–1) of the map image and scaled to
  its rendered size at draw time, so the overlay stays aligned under
  `object-fit: contain` at any window size.
- `public/zone_map.png` is rendered from `Zone Map.pdf`.
- The zone tint rectangles are rough regions for orientation, not real ward
  boundaries.
