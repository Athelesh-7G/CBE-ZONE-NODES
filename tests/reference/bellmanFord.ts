/**
 * Independent Bellman-Ford reference for tests. Shares no code with
 * lib/dijkstra.ts. Edges are undirected, so each is relaxed in both directions.
 * Returns shortest distances from `source` to every node (Infinity if unreachable).
 */
export function bellmanFord(
  nodes: string[],
  edges: Array<[string, string, number]>,
  source: string
): Record<string, number> {
  const dist: Record<string, number> = {}
  for (const n of nodes) dist[n] = Infinity
  dist[source] = 0

  for (let round = 0; round < nodes.length - 1; round++) {
    let changed = false
    for (const [a, b, w] of edges) {
      if (dist[a] + w < dist[b]) {
        dist[b] = dist[a] + w
        changed = true
      }
      if (dist[b] + w < dist[a]) {
        dist[a] = dist[b] + w
        changed = true
      }
    }
    if (!changed) break
  }
  return dist
}
