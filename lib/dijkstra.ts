export interface DijkstraResult {
  path: string[] // node ids in order, source -> destination
  distances: Record<string, number> // shortest distance from source to each node
  visited: string[] // order in which nodes were settled
  totalDistance: number
  found: boolean
  steps: DijkstraStep[] // trace of each settle operation, for the step table
}

export interface NeighborRelaxation {
  id: string
  weight: number
  oldDistance: number
  newDistance: number
  improved: boolean
}

export interface DijkstraStep {
  order: number
  current: string
  currentDistance: number
  neighbors: NeighborRelaxation[]
  distances: Record<string, number> // snapshot after this step
}

type Entry = { id: string; dist: number }

/**
 * Min-priority queue backed by a sorted array. Insertion is O(n), but n is the
 * node count of a city map, so the constant factor beats a real heap here.
 */
class SortedQueue {
  private items: Entry[] = []

  push(entry: Entry) {
    let lo = 0
    let hi = this.items.length
    while (lo < hi) {
      const mid = (lo + hi) >> 1
      if (this.items[mid].dist <= entry.dist) lo = mid + 1
      else hi = mid
    }
    this.items.splice(lo, 0, entry)
  }

  pop(): Entry | undefined {
    return this.items.shift()
  }

  get size() {
    return this.items.length
  }
}

/**
 * Dijkstra's shortest path over an undirected, non-negatively weighted graph.
 * Edges are treated as bidirectional. Unreachable nodes keep a distance of
 * Infinity and never enter `visited`.
 */
export function dijkstra(
  nodes: string[],
  edges: Array<[string, string, number]>,
  source: string,
  destination: string
): DijkstraResult {
  const empty: DijkstraResult = {
    path: [],
    distances: {},
    visited: [],
    totalDistance: 0,
    found: false,
    steps: [],
  }

  if (!nodes.includes(source) || !nodes.includes(destination)) return empty

  // Adjacency list. Parallel edges collapse to the cheapest one.
  const adjacency = new Map<string, Map<string, number>>()
  for (const id of nodes) adjacency.set(id, new Map())
  for (const [a, b, weight] of edges) {
    const fromA = adjacency.get(a)
    const fromB = adjacency.get(b)
    if (!fromA || !fromB || weight < 0) continue
    const existingA = fromA.get(b)
    if (existingA === undefined || weight < existingA) fromA.set(b, weight)
    const existingB = fromB.get(a)
    if (existingB === undefined || weight < existingB) fromB.set(a, weight)
  }

  const distances: Record<string, number> = {}
  const previous: Record<string, string | null> = {}
  const settled = new Set<string>()
  const visited: string[] = []
  const steps: DijkstraStep[] = []

  for (const id of nodes) {
    distances[id] = Infinity
    previous[id] = null
  }
  distances[source] = 0

  const queue = new SortedQueue()
  queue.push({ id: source, dist: 0 })

  while (queue.size > 0) {
    const entry = queue.pop()!
    // Stale entry left behind by an earlier, longer tentative distance.
    if (settled.has(entry.id)) continue
    settled.add(entry.id)
    visited.push(entry.id)

    const relaxations: NeighborRelaxation[] = []
    const neighbors = adjacency.get(entry.id) ?? new Map()

    for (const [neighborId, weight] of neighbors) {
      const oldDistance = distances[neighborId]
      const candidate = distances[entry.id] + weight
      const improved = !settled.has(neighborId) && candidate < oldDistance

      if (improved) {
        distances[neighborId] = candidate
        previous[neighborId] = entry.id
        queue.push({ id: neighborId, dist: candidate })
      }

      relaxations.push({
        id: neighborId,
        weight,
        oldDistance,
        newDistance: distances[neighborId],
        improved,
      })
    }

    steps.push({
      order: steps.length + 1,
      current: entry.id,
      currentDistance: distances[entry.id],
      neighbors: relaxations,
      distances: { ...distances },
    })

    // The destination's distance is final once it is settled.
    if (entry.id === destination) break
  }

  if (!settled.has(destination)) {
    return { path: [], distances, visited, totalDistance: 0, found: false, steps }
  }

  const path: string[] = []
  for (let at: string | null = destination; at !== null; at = previous[at]) {
    path.unshift(at)
  }

  return {
    path,
    distances,
    visited,
    totalDistance: distances[destination],
    found: true,
    steps,
  }
}

/** Distance of a single hop, used by the sidebar's per-leg breakdown. */
export function edgeWeight(
  edges: Array<[string, string, number]>,
  a: string,
  b: string
): number {
  let best = Infinity
  for (const [from, to, weight] of edges) {
    if ((from === a && to === b) || (from === b && to === a)) {
      if (weight < best) best = weight
    }
  }
  return best === Infinity ? 0 : best
}
