"use client"

import { useCallback, useEffect, useMemo, useRef, useState } from "react"
import { dijkstra, edgeWeight, type DijkstraResult } from "@/lib/dijkstra"
import { EDGES, NODES, NODE_BY_ID, ZONES, nodeName } from "@/lib/graph"

const VISIT_DELAY = 80 // ms between each settled node
const PATH_PAUSE = 200 // ms pause before the path animates
const PATH_DELAY = 150 // ms between each path edge

const COLOR = {
  edge: "#444444",
  path: "#22c55e",
  source: "#22c55e",
  destination: "#ef4444",
  waypoint: "#eab308",
  visited: "#60a5fa",
  idle: "#ffffff",
}

/** Geometry of the map image inside its container under `object-fit: contain`. */
interface ImageRect {
  left: number
  top: number
  width: number
  height: number
}

function containRect(
  containerW: number,
  containerH: number,
  naturalW: number,
  naturalH: number
): ImageRect {
  if (!containerW || !containerH || !naturalW || !naturalH) {
    return { left: 0, top: 0, width: 0, height: 0 }
  }
  const scale = Math.min(containerW / naturalW, containerH / naturalH)
  const width = naturalW * scale
  const height = naturalH * scale
  return {
    left: (containerW - width) / 2,
    top: (containerH - height) / 2,
    width,
    height,
  }
}

const edgeKey = (a: string, b: string) => [a, b].sort().join("|")

export default function Page() {
  const [source, setSource] = useState("thudiyalur")
  const [destination, setDestination] = useState("seeranapalayam")
  const [showAllEdges, setShowAllEdges] = useState(true)
  const [showSteps, setShowSteps] = useState(false)

  const [result, setResult] = useState<DijkstraResult | null>(null)
  const [visitedCount, setVisitedCount] = useState(0)
  const [pathEdgeCount, setPathEdgeCount] = useState(0)
  const [running, setRunning] = useState(false)
  const [showResults, setShowResults] = useState(false)
  const [hovered, setHovered] = useState<{ id: string; x: number; y: number } | null>(
    null
  )

  const [selected, setSelected] = useState<string | null>(null)

  const timers = useRef<ReturnType<typeof setTimeout>[]>([])
  const containerRef = useRef<HTMLDivElement>(null)
  const [containerSize, setContainerSize] = useState({ w: 0, h: 0 })
  const [natural, setNatural] = useState({ w: 3000, h: 2122 })

  // Resolved on the client only: `window` does not exist during prerender, and
  // reading it while rendering would also desync hydration.
  const [isTouchDevice, setIsTouchDevice] = useState(false)
  useEffect(() => {
    setIsTouchDevice("ontouchstart" in window)
  }, [])

  const clearTimers = useCallback(() => {
    timers.current.forEach(clearTimeout)
    timers.current = []
  }, [])

  useEffect(() => clearTimers, [clearTimers])

  // Track the container so node positions follow window resizes.
  useEffect(() => {
    const el = containerRef.current
    if (!el) return
    const observer = new ResizeObserver(([entry]) => {
      const { width, height } = entry.contentRect
      setContainerSize({ w: width, h: height })
    })
    observer.observe(el)
    return () => observer.disconnect()
  }, [])

  const rect = useMemo(
    () => containRect(containerSize.w, containerSize.h, natural.w, natural.h),
    [containerSize, natural]
  )

  const px = useCallback((fx: number) => rect.left + fx * rect.width, [rect])
  const py = useCallback((fy: number) => rect.top + fy * rect.height, [rect])

  // Until the container has been measured the rect is empty, which would stack
  // every node at the origin. Hold the overlay back for that first frame.
  const measured = rect.width > 0 && rect.height > 0

  // Narrow map area: thin out the overlay so the markers stay legible. Derived
  // from the container rather than the viewport, since the map is what crowds.
  const isMobile = measured && containerSize.w < 500
  // Shrink markers proportionally below 500px, bottoming out at 0.8 (r 10 -> 8).
  const markerScale = isMobile
    ? Math.max(0.8, Math.min(1, containerSize.w / 500))
    : 1
  const nodeRadius = 10 * markerScale
  const keyRadius = 13 * markerScale

  const reset = useCallback(() => {
    clearTimers()
    setResult(null)
    setVisitedCount(0)
    setPathEdgeCount(0)
    setRunning(false)
    setShowResults(false)
    setShowSteps(false)
    setSelected(null)
  }, [clearTimers])

  const run = useCallback(() => {
    clearTimers()
    const res = dijkstra(
      NODES.map((n) => n.id),
      EDGES,
      source,
      destination
    )

    setResult(res)
    setVisitedCount(0)
    setPathEdgeCount(0)
    setShowResults(false)
    setRunning(true)

    const schedule = (fn: () => void, delay: number) => {
      timers.current.push(setTimeout(fn, delay))
    }

    // 1. Reveal settled nodes in visit order.
    res.visited.forEach((_, i) => {
      schedule(() => setVisitedCount(i + 1), i * VISIT_DELAY)
    })

    const afterVisits = res.visited.length * VISIT_DELAY + PATH_PAUSE

    if (!res.found) {
      schedule(() => {
        setShowResults(true)
        setRunning(false)
      }, afterVisits)
      return
    }

    // 2. Trace the shortest path one edge at a time.
    const edgeCount = Math.max(res.path.length - 1, 0)
    for (let i = 0; i < edgeCount; i++) {
      schedule(() => setPathEdgeCount(i + 1), afterVisits + i * PATH_DELAY)
    }

    // 3. Reveal the results panel once the animation lands.
    schedule(() => {
      setShowResults(true)
      setRunning(false)
    }, afterVisits + edgeCount * PATH_DELAY + 150)
  }, [clearTimers, source, destination])

  // Nodes settled so far in the current animation.
  const visitedSoFar = useMemo(
    () => new Set(result ? result.visited.slice(0, visitedCount) : []),
    [result, visitedCount]
  )

  // Path edges revealed so far, keyed for O(1) lookup while rendering.
  const revealedPathEdges = useMemo(() => {
    const set = new Set<string>()
    if (!result?.found) return set
    for (let i = 0; i < pathEdgeCount && i < result.path.length - 1; i++) {
      set.add(edgeKey(result.path[i], result.path[i + 1]))
    }
    return set
  }, [result, pathEdgeCount])

  // Orientation of each path edge along the walk, so the draw animation always
  // runs source -> destination rather than following the raw EDGES order.
  const pathEdgeDirection = useMemo(() => {
    const map = new Map<string, { from: string; to: string }>()
    if (!result?.found) return map
    for (let i = 0; i < result.path.length - 1; i++) {
      const from = result.path[i]
      const to = result.path[i + 1]
      map.set(edgeKey(from, to), { from, to })
    }
    return map
  }, [result])

  // A path node lights up once the edge leading into it has been drawn.
  const revealedPathNodes = useMemo(() => {
    const set = new Set<string>()
    if (!result?.found) return set
    for (let i = 0; i <= pathEdgeCount && i < result.path.length; i++) {
      set.add(result.path[i])
    }
    return set
  }, [result, pathEdgeCount])

  const nodeFill = (id: string) => {
    if (id === source) return COLOR.source
    if (id === destination) return COLOR.destination
    if (revealedPathNodes.has(id)) return COLOR.waypoint
    if (visitedSoFar.has(id)) return COLOR.visited
    return COLOR.idle
  }

  const distanceFromSource = (id: string) => {
    if (!result) return null
    const d = result.distances[id]
    return d === undefined || d === Infinity ? null : d
  }

  const noPath = showResults && result && !result.found

  return (
    <main className="flex min-h-screen w-full flex-col bg-[#0f0f0f] text-neutral-200 md:h-screen md:w-screen md:flex-row md:overflow-hidden">
      {/* ---------------- Sidebar ---------------- */}
      <aside className="order-2 flex max-h-[45vh] w-full flex-col gap-4 overflow-y-auto border-t border-neutral-800 bg-[#141414] p-3 md:order-1 md:h-full md:max-h-none md:w-[280px] md:shrink-0 md:gap-5 md:border-r md:border-t-0 md:p-5">
        <header className="flex items-center gap-2 md:gap-3">
          <GraphIcon />
          <div>
            <h1 className="text-sm font-semibold leading-tight text-white md:text-base">
              CBE Zone Map
            </h1>
            <p className="text-[11px] text-neutral-500 md:text-xs">
              Dijkstra&apos;s Algorithm
            </p>
          </div>
        </header>

        <div className="grid grid-cols-2 gap-2 md:grid-cols-1 md:gap-3">
          <Select
            label="Source"
            value={source}
            onChange={setSource}
            disabled={running}
          />
          <Select
            label="Destination"
            value={destination}
            onChange={setDestination}
            disabled={running}
          />
        </div>

        <div className="space-y-2">
          <div className="grid grid-cols-2 gap-2 md:grid-cols-1">
            <button
              onClick={run}
              disabled={running || source === destination}
              className="flex min-h-[44px] w-full items-center justify-center gap-2 rounded-md bg-[#22c55e] px-3 py-2 text-sm font-semibold text-black transition-colors duration-150 hover:bg-[#16a34a] disabled:cursor-not-allowed disabled:opacity-40 md:min-h-0"
            >
              {running ? (
                <>
                  <span className="h-3.5 w-3.5 animate-spin rounded-full border-2 border-black/25 border-t-black" />
                  Running…
                </>
              ) : (
                "Find Shortest Path"
              )}
            </button>
            <button
              onClick={reset}
              className="min-h-[44px] w-full rounded-md border border-neutral-700 bg-neutral-800 px-3 py-2 text-sm font-medium text-neutral-300 transition-colors duration-150 hover:bg-neutral-700 md:min-h-0"
            >
              Reset
            </button>
          </div>
          <button
            onClick={() => setShowSteps(true)}
            disabled={!result?.steps.length}
            className="min-h-[44px] w-full rounded-md border border-neutral-700 px-3 py-2 text-sm font-medium text-neutral-400 transition-colors duration-150 hover:bg-neutral-800 disabled:cursor-not-allowed disabled:opacity-40 md:min-h-0"
          >
            Step-by-step Table
          </button>
        </div>

        <label className="flex min-h-[44px] cursor-pointer items-center gap-2 text-sm text-neutral-400 md:min-h-0">
          <input
            type="checkbox"
            checked={showAllEdges}
            onChange={(e) => setShowAllEdges(e.target.checked)}
            className="h-5 w-5 accent-[#22c55e] md:h-4 md:w-4"
          />
          Show All Edges
          {isMobile && (
            <span className="text-[10px] text-neutral-600">(weights: path only)</span>
          )}
        </label>

        {source === destination && (
          <p className="rounded-md bg-amber-500/10 p-2 text-xs text-amber-400">
            Source and destination are the same.
          </p>
        )}

        {noPath && (
          <p className="rounded-md bg-red-500/10 p-2 text-xs text-red-400">
            No path exists between {nodeName(source)} and {nodeName(destination)}.
          </p>
        )}

        {/* ---------------- Empty state ---------------- */}
        {!result && (
          <div className="flex flex-col items-center gap-2 rounded-lg border border-dashed border-neutral-800 bg-neutral-900/40 px-4 py-6 text-center">
            <RouteIcon />
            <p className="text-xs leading-relaxed text-neutral-500">
              Select a source and destination, then click Find Shortest Path
            </p>
          </div>
        )}

        {/* ---------------- Results ---------------- */}
        {showResults && result?.found && (
          <section className="fade-in space-y-3 border-t border-neutral-800 pt-4">
            <div>
              <h2 className="mb-1.5 text-xs font-semibold uppercase tracking-wide text-neutral-500">
                Shortest Path
              </h2>
              <div className="flex flex-wrap items-center gap-x-1 gap-y-1.5">
                {result.path.map((id, i) => (
                  <span key={id} className="flex items-center gap-1">
                    {i > 0 && <span className="text-neutral-600">→</span>}
                    <span className="rounded-full bg-[#22c55e]/15 px-2 py-0.5 text-xs font-medium text-[#22c55e] md:text-[11px]">
                      {nodeName(id)}
                    </span>
                  </span>
                ))}
              </div>
            </div>

            <div className="rounded-md bg-[#22c55e]/10 p-3">
              <p className="text-xs text-neutral-400">Total Distance</p>
              <p className="text-2xl font-bold leading-tight text-[#22c55e] md:text-[32px]">
                {result.totalDistance}
                <span className="ml-1 text-sm font-semibold md:text-base">km</span>
              </p>
            </div>

            <div>
              <h2 className="mb-2 text-xs font-semibold uppercase tracking-wide text-neutral-500">
                Steps
              </h2>
              <ol className="space-y-1.5">
                {result.path.map((id, i) => {
                  const leg = i === 0 ? 0 : edgeWeight(EDGES, result.path[i - 1], id)
                  return (
                    <li
                      key={id}
                      className="flex items-baseline justify-between gap-2 text-xs"
                    >
                      <span className="text-neutral-300">
                        <span className="mr-1.5 text-neutral-600">{i + 1}.</span>
                        {nodeName(id)}
                      </span>
                      <span className="shrink-0 font-mono text-neutral-500">
                        {i === 0 ? "start" : `+${leg} km`}
                      </span>
                    </li>
                  )
                })}
              </ol>
            </div>

            <p className="text-xs text-neutral-600">
              Visited {result.visited.length} of {NODES.length} nodes
            </p>
          </section>
        )}

        {/* Horizontal scroller on mobile, stacked list on desktop. */}
        <div className="mt-auto flex gap-3 overflow-x-auto border-t border-neutral-800 pt-3 text-[11px] text-neutral-500 md:flex-col md:gap-1.5 md:overflow-x-visible md:pt-4">
          <Legend color={COLOR.source} label="Source" />
          <Legend color={COLOR.destination} label="Destination" />
          <Legend color={COLOR.waypoint} label="On path" />
          <Legend color={COLOR.visited} label="Visited" />
        </div>
      </aside>

      {/* ---------------- Map ---------------- */}
      <div
        ref={containerRef}
        className="relative order-1 min-h-[55vw] w-full flex-1 md:order-2 md:h-full md:min-h-0"
      >
        {/* eslint-disable-next-line @next/next/no-img-element */}
        <img
          src="/zone_map.png"
          alt="Coimbatore City Municipal Corporation zone map"
          className="absolute inset-0 h-full w-full object-contain"
          onLoad={(e) => {
            const img = e.currentTarget
            setNatural({ w: img.naturalWidth, h: img.naturalHeight })
          }}
        />

        <svg className="absolute inset-0 h-full w-full">
          <defs>
            <filter id="glow" x="-50%" y="-50%" width="200%" height="200%">
              <feGaussianBlur stdDeviation="3" result="blur" />
              <feMerge>
                <feMergeNode in="blur" />
                <feMergeNode in="SourceGraphic" />
              </feMerge>
            </filter>
          </defs>

          {measured && (
            <>
          {/* Approximate zone tints, behind everything else. */}
          {ZONES.map((z) => (
            <rect
              key={z.name}
              x={px(z.x)}
              y={py(z.y)}
              width={z.w * rect.width}
              height={z.h * rect.height}
              rx={12}
              fill={z.color}
              opacity={0.1}
            />
          ))}

          {/* Edges */}
          {EDGES.map(([a, b, w]) => {
            const key = edgeKey(a, b)
            const onPath = revealedPathEdges.has(key)
            // Draw path edges in walk order so the dash unspools the right way.
            const dir = pathEdgeDirection.get(key)
            const [fromId, toId] = onPath && dir ? [dir.from, dir.to] : [a, b]
            const na = NODE_BY_ID[fromId]
            const nb = NODE_BY_ID[toId]
            const x1 = px(na.x)
            const y1 = py(na.y)
            const x2 = px(nb.x)
            const y2 = py(nb.y)
            const length = Math.hypot(x2 - x1, y2 - y1)
            return (
              <g key={key}>
                <line
                  x1={x1}
                  y1={y1}
                  x2={x2}
                  y2={y2}
                  stroke={onPath ? COLOR.path : COLOR.edge}
                  strokeWidth={onPath ? 3 : 1.25}
                  strokeLinecap="round"
                  filter={onPath ? "url(#glow)" : undefined}
                  opacity={onPath ? 1 : 0.85}
                  className={onPath ? "edge-draw" : undefined}
                  style={
                    onPath
                      ? ({
                          strokeDasharray: length,
                          "--edge-len": length,
                        } as React.CSSProperties)
                      : undefined
                  }
                />
                {/* On a narrow map the weight pills are unreadable clutter, so
                    only the path's own weights survive there. */}
                {((showAllEdges && !isMobile) || onPath) && (
                  <WeightLabel
                    x={(x1 + x2) / 2}
                    y={(y1 + y2) / 2}
                    value={w}
                    highlighted={onPath}
                  />
                )}
              </g>
            )
          })}

          {/* Nodes */}
          {NODES.map((n) => {
            const cx = px(n.x)
            const cy = py(n.y)
            const isKey = n.id === source || n.id === destination
            const fill = nodeFill(n.id)
            const r = isKey ? keyRadius : nodeRadius
            // Narrow maps only caption the nodes that carry meaning; the rest
            // would overlap into an unreadable mess.
            const showLabel = !isMobile || isKey || revealedPathNodes.has(n.id)
            return (
              <g
                key={n.id}
                onMouseEnter={
                  isTouchDevice
                    ? undefined
                    : () => setHovered({ id: n.id, x: cx, y: cy })
                }
                onMouseLeave={isTouchDevice ? undefined : () => setHovered(null)}
                onClick={() => setSelected(n.id)}
                className="cursor-pointer"
              >
                {isKey && (
                  <circle
                    cx={cx}
                    cy={cy}
                    r={keyRadius}
                    fill="none"
                    stroke={fill}
                    strokeWidth={2}
                    className="pulse-ring"
                  />
                )}
                <circle
                  cx={cx}
                  cy={cy}
                  r={r}
                  fill={fill}
                  stroke="#0f0f0f"
                  strokeWidth={2}
                  className="node-circle"
                />
                {showLabel && (
                  <NodeLabel
                    x={cx}
                    y={cy}
                    text={n.name}
                    offset={isKey ? 3 : 0}
                    compact={isMobile}
                  />
                )}
                {/* Generous invisible hit/tap area. */}
                <circle cx={cx} cy={cy} r={isMobile ? 16 : 18} fill="transparent" />
              </g>
            )
          })}
            </>
          )}
        </svg>

        {/* Hover tooltip — pointer devices only; touch uses the info bar. */}
        {hovered && !isTouchDevice && (
          <div
            className="pointer-events-none absolute z-10 -translate-x-1/2 -translate-y-full rounded-md border border-neutral-700 bg-neutral-900/95 px-2.5 py-1.5 text-xs shadow-xl"
            style={{ left: hovered.x, top: hovered.y - 34 }}
          >
            <p className="font-semibold text-white">{nodeName(hovered.id)}</p>
            {result && (
              <p className="text-neutral-400">
                {distanceFromSource(hovered.id) !== null
                  ? `${distanceFromSource(hovered.id)} km from ${nodeName(source)}`
                  : "Unreachable"}
              </p>
            )}
          </div>
        )}

        {/* Touch info bar — the tap-driven stand-in for the hover tooltip. */}
        {selected && isTouchDevice && (
          <div className="absolute inset-x-0 bottom-0 z-10 flex items-center justify-between gap-3 border-t border-neutral-800 bg-neutral-900/95 px-3 py-2 text-xs backdrop-blur-sm">
            <p className="truncate">
              <span className="font-semibold text-white">{nodeName(selected)}</span>
              {result && (
                <span className="text-neutral-400">
                  {distanceFromSource(selected) !== null
                    ? ` — ${distanceFromSource(selected)} km from ${nodeName(source)}`
                    : " — unreachable"}
                </span>
              )}
            </p>
            <button
              onClick={() => setSelected(null)}
              aria-label="Dismiss"
              className="shrink-0 rounded px-2 py-1 text-neutral-500 hover:bg-neutral-800"
            >
              ✕
            </button>
          </div>
        )}
      </div>

      {/* ---------------- Step table modal ---------------- */}
      {showSteps && result && (
        <div
          className="fixed inset-0 z-20 flex items-center justify-center bg-black/70 p-3 md:p-8"
          onClick={() => setShowSteps(false)}
        >
          <div
            className="flex max-h-full w-full max-w-3xl flex-col overflow-hidden rounded-lg border border-neutral-800 bg-[#141414]"
            onClick={(e) => e.stopPropagation()}
          >
            <header className="flex items-center justify-between gap-2 border-b border-neutral-800 px-3 py-3 md:px-5">
              <div className="min-w-0">
                <h2 className="text-sm font-semibold text-white">Algorithm Trace</h2>
                <p className="truncate text-[11px] text-neutral-500 md:text-xs">
                  {nodeName(source)} → {nodeName(destination)} · each row is one
                  settled node
                </p>
              </div>
              <button
                onClick={() => setShowSteps(false)}
                className="min-h-[44px] shrink-0 rounded px-3 py-1 text-sm text-neutral-400 hover:bg-neutral-800 md:min-h-0 md:px-2"
              >
                Close
              </button>
            </header>

            {/* The trace is too wide for a phone; let it scroll sideways
                rather than crushing the columns. */}
            <div className="overflow-auto">
              <table className="w-full min-w-[560px] border-collapse text-left text-xs md:min-w-0">
                <thead className="sticky top-0 bg-[#1c1c1c] text-neutral-400">
                  <tr>
                    <th className="px-4 py-2 font-medium">#</th>
                    <th className="px-4 py-2 font-medium">Current</th>
                    <th className="px-4 py-2 font-medium">Dist</th>
                    <th className="px-4 py-2 font-medium">
                      Tentative distances to neighbours
                    </th>
                  </tr>
                </thead>
                <tbody>
                  {result.steps.map((step, i) => {
                    // Rows for nodes that ended up on the shortest path get a
                    // green tint; everything else alternates for readability.
                    const onPath = result.path.includes(step.current)
                    return (
                    <tr
                      key={step.order}
                      className={`border-t border-neutral-800 align-top ${
                        onPath
                          ? "bg-[#22c55e]/10"
                          : i % 2 === 1
                            ? "bg-white/[0.02]"
                            : ""
                      }`}
                    >
                      <td className="px-4 py-2 font-mono text-neutral-600">
                        {step.order}
                      </td>
                      <td
                        className={`px-4 py-2 font-medium ${
                          onPath ? "text-[#22c55e]" : "text-white"
                        }`}
                      >
                        {nodeName(step.current)}
                      </td>
                      <td className="px-4 py-2 font-mono text-[#22c55e]">
                        {step.currentDistance}
                      </td>
                      <td className="px-4 py-2">
                        <div className="flex flex-wrap gap-1.5">
                          {step.neighbors.map((nb) => (
                            <span
                              key={nb.id}
                              className={`rounded px-1.5 py-0.5 font-mono ${
                                nb.improved
                                  ? "bg-[#22c55e]/15 text-[#22c55e]"
                                  : "bg-neutral-800 text-neutral-500"
                              }`}
                              title={
                                nb.improved
                                  ? `Updated ${fmt(nb.oldDistance)} → ${nb.newDistance}`
                                  : "No improvement"
                              }
                            >
                              {nodeName(nb.id)} ({nb.weight}) = {fmt(nb.newDistance)}
                            </span>
                          ))}
                        </div>
                      </td>
                    </tr>
                    )
                  })}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </main>
  )
}

const fmt = (n: number) => (n === Infinity ? "∞" : String(n))

function Select({
  label,
  value,
  onChange,
  disabled,
}: {
  label: string
  value: string
  onChange: (v: string) => void
  disabled?: boolean
}) {
  return (
    <label className="block">
      <span className="mb-1 block text-xs font-medium text-neutral-500">{label}</span>
      {/* 16px on mobile: iOS auto-zooms any select under that size on focus. */}
      <select
        value={value}
        disabled={disabled}
        onChange={(e) => onChange(e.target.value)}
        className="min-h-[44px] w-full rounded-md border border-neutral-700 bg-neutral-900 px-2.5 py-2 text-base text-neutral-100 outline-none transition focus:border-[#22c55e] disabled:opacity-50 md:min-h-0 md:text-sm"
      >
        {NODES.map((n) => (
          <option key={n.id} value={n.id}>
            {n.name}
          </option>
        ))}
      </select>
    </label>
  )
}

function Legend({ color, label }: { color: string; label: string }) {
  return (
    <div className="flex shrink-0 items-center gap-2 whitespace-nowrap">
      <span
        className="inline-block h-2.5 w-2.5 shrink-0 rounded-full"
        style={{ backgroundColor: color }}
      />
      {label}
    </div>
  )
}

/**
 * Node caption with a dark pill so it stays readable over the map. `offset`
 * pushes the pill clear of the larger source/destination circles.
 */
function NodeLabel({
  x,
  y,
  text,
  offset = 0,
  compact = false,
}: {
  x: number
  y: number
  text: string
  offset?: number
  compact?: boolean
}) {
  const fontSize = compact ? 9 : 11
  const height = compact ? 13 : 15
  const width = text.length * (compact ? 4.7 : 5.6) + 10
  const top = y + (compact ? 11 : 13) + offset
  return (
    <>
      <rect
        x={x - width / 2}
        y={top}
        width={width}
        height={height}
        rx={height / 2}
        fill="#0f0f0f"
        opacity={0.78}
      />
      <text
        x={x}
        y={top + height / 2 + fontSize / 3}
        textAnchor="middle"
        fontSize={fontSize}
        fill="#ffffff"
        style={{ userSelect: "none" }}
      >
        {text}
      </text>
    </>
  )
}

function WeightLabel({
  x,
  y,
  value,
  highlighted,
}: {
  x: number
  y: number
  value: number
  highlighted: boolean
}) {
  return (
    <>
      <rect
        x={x - 11}
        y={y - 8}
        width={22}
        height={14}
        rx={4}
        fill="#0f0f0f"
        opacity={highlighted ? 0.85 : 0.6}
      />
      <text
        x={x}
        y={y + 2}
        textAnchor="middle"
        fontSize={10}
        fill={highlighted ? COLOR.path : "#9ca3af"}
        fontWeight={highlighted ? 700 : 400}
        style={{ userSelect: "none" }}
      >
        {value}
      </text>
    </>
  )
}

/** Placeholder icon for the empty results state. */
function RouteIcon() {
  return (
    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <path
        d="M6 18 C 6 12, 18 12, 18 6"
        stroke="#3f3f46"
        strokeWidth="1.5"
        strokeDasharray="3 2.5"
      />
      <circle cx="6" cy="18" r="2.5" fill="#3f3f46" />
      <circle cx="18" cy="6" r="2.5" fill="#3f3f46" />
    </svg>
  )
}

function GraphIcon() {
  return (
    <svg width="26" height="26" viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <path d="M6 6 L12 17 L18 8" stroke="#22c55e" strokeWidth="1.5" />
      <path d="M6 6 L18 8" stroke="#3f3f46" strokeWidth="1.5" />
      <circle cx="6" cy="6" r="3" fill="#22c55e" />
      <circle cx="18" cy="8" r="3" fill="#eab308" />
      <circle cx="12" cy="17" r="3" fill="#ef4444" />
    </svg>
  )
}
