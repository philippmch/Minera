// Hash routes for the open tool or log view, so the browser's Back and Forward
// buttons move between pages inside the app and a tool has a link of its own.
//   #/tool/<tool-id>   a standalone tool (works without an open log)
//   #/view/<view-id>   a view of the open event log
//   #/workspace        the object-centric or source workspace of open data
//   (empty)            the start page
// A view or workspace route only means something while data is open; after a
// reload, which drops the open data, those routes fall back to the start page.

import type { Tab } from "./app-logic";
import { TOOLS, VIEWS, type ToolId } from "./tools";

export type Route =
  | { kind: "home" }
  | { kind: "tool"; id: ToolId }
  | { kind: "view"; id: Tab }
  | { kind: "workspace" };

const TOOL_IDS = new Set<string>(TOOLS.map((tool) => tool.id));
const VIEW_IDS = new Set<string>(VIEWS.map((view) => view.id));

export function parseRoute(hash: string): Route {
  const path = hash.replace(/^#/, "").replace(/^\/+/, "").replace(/\/+$/, "");
  const [kind, raw, ...rest] = path.split("/");
  if (rest.length) return { kind: "home" };
  let id = "";
  try { id = decodeURIComponent(raw ?? ""); } catch { return { kind: "home" }; }
  if (kind === "tool" && TOOL_IDS.has(id)) return { kind: "tool", id: id as ToolId };
  if (kind === "view" && VIEW_IDS.has(id)) return { kind: "view", id: id as Tab };
  if (kind === "workspace" && raw === undefined) return { kind: "workspace" };
  return { kind: "home" };
}

/** The location hash for a route; the start page has none. */
export function routeHash(route: Route): string {
  switch (route.kind) {
    case "home": return "";
    case "tool": return `#/tool/${encodeURIComponent(route.id)}`;
    case "view": return `#/view/${encodeURIComponent(route.id)}`;
    case "workspace": return "#/workspace";
  }
}

/** The route of what the app shows: an open tool wins, then the log view, then other open data. */
export function routeFor(state: { tool: ToolId | null; view: Tab | null; hasLog: boolean; hasOtherData: boolean }): Route {
  if (state.tool) return { kind: "tool", id: state.tool };
  if (state.hasLog && state.view) return { kind: "view", id: state.view };
  if (state.hasLog || state.hasOtherData) return { kind: "workspace" };
  return { kind: "home" };
}

/** The tool a fresh page load opens; view and workspace routes need data, which a reload does not keep. */
export function initialToolFromHash(hash: string): ToolId | null {
  const route = parseRoute(hash);
  return route.kind === "tool" ? route.id : null;
}

/** Normalises a hash for comparison ("#" and "" both mean the start page). */
export function sameHash(a: string, b: string): boolean {
  const clean = (value: string) => (value === "#" ? "" : value);
  return clean(a) === clean(b);
}
