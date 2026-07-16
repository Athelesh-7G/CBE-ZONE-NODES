export interface MapNode {
  id: string
  name: string
  /** Fractions (0-1) of the rendered map image's width/height. */
  x: number
  y: number
}

export const NODES: MapNode[] = [
  { id: "sulekkai", name: "Sulekkai", x: 0.48, y: 0.1 },
  { id: "thudiyalur", name: "Thudiyalur", x: 0.33, y: 0.22 },
  { id: "saravanampatti", name: "Saravanampatti", x: 0.6, y: 0.18 },
  { id: "kalapatti", name: "Kalapatti", x: 0.65, y: 0.28 },
  { id: "gandhipuram", name: "Gandhipuram", x: 0.44, y: 0.36 },
  { id: "peelamedu", name: "Peelamedu", x: 0.68, y: 0.38 },
  { id: "neelambur", name: "Neelambur", x: 0.82, y: 0.4 },
  { id: "rs_puram", name: "RS Puram", x: 0.38, y: 0.46 },
  { id: "townhall", name: "Town Hall", x: 0.48, y: 0.48 },
  { id: "singanallur", name: "Singanallur", x: 0.68, y: 0.52 },
  { id: "vedapatti", name: "Vedapatti", x: 0.22, y: 0.52 },
  { id: "ukkadam", name: "Ukkadam", x: 0.5, y: 0.6 },
  { id: "kurichi", name: "Kurichi", x: 0.42, y: 0.68 },
  { id: "perur", name: "Perur", x: 0.32, y: 0.72 },
  { id: "pattanam", name: "Pattanam", x: 0.68, y: 0.72 },
  { id: "seeranapalayam", name: "Seeranapalayam", x: 0.48, y: 0.85 },
]

export const EDGES: Array<[string, string, number]> = [
  ["sulekkai", "thudiyalur", 7],
  ["sulekkai", "saravanampatti", 8],
  ["sulekkai", "gandhipuram", 6],
  ["thudiyalur", "gandhipuram", 7],
  ["thudiyalur", "vedapatti", 9],
  ["saravanampatti", "kalapatti", 5],
  ["saravanampatti", "gandhipuram", 10],
  ["kalapatti", "peelamedu", 6],
  ["kalapatti", "gandhipuram", 9],
  ["gandhipuram", "townhall", 2],
  ["gandhipuram", "rs_puram", 4],
  ["peelamedu", "neelambur", 8],
  ["peelamedu", "singanallur", 7],
  ["peelamedu", "townhall", 8],
  ["rs_puram", "townhall", 3],
  ["rs_puram", "vedapatti", 5],
  ["rs_puram", "perur", 7],
  ["townhall", "ukkadam", 4],
  ["townhall", "singanallur", 9],
  ["singanallur", "ukkadam", 6],
  ["singanallur", "pattanam", 9],
  ["vedapatti", "perur", 8],
  ["ukkadam", "kurichi", 5],
  ["ukkadam", "pattanam", 7],
  ["kurichi", "perur", 4],
  ["kurichi", "seeranapalayam", 6],
  ["perur", "seeranapalayam", 8],
  ["pattanam", "seeranapalayam", 10],
]

export const NODE_BY_ID: Record<string, MapNode> = Object.fromEntries(
  NODES.map((node) => [node.id, node])
)

export const nodeName = (id: string) => NODE_BY_ID[id]?.name ?? id

/**
 * Approximate zone regions for the background tint. These are rough rectangles
 * over the corporation's five zones, not real ward boundaries.
 */
export interface Zone {
  name: string
  color: string
  x: number
  y: number
  w: number
  h: number
}

export const ZONES: Zone[] = [
  { name: "North", color: "#3b82f6", x: 0.34, y: 0.12, w: 0.28, h: 0.34 },
  { name: "East", color: "#a855f7", x: 0.6, y: 0.16, w: 0.26, h: 0.42 },
  { name: "West", color: "#f97316", x: 0.13, y: 0.32, w: 0.26, h: 0.3 },
  { name: "Central", color: "#9ca3af", x: 0.39, y: 0.46, w: 0.22, h: 0.22 },
  { name: "South", color: "#22c55e", x: 0.28, y: 0.64, w: 0.3, h: 0.3 },
]
