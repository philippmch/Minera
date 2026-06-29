import { type FilterOptions } from "./api";
import { type MineraExport } from "./types";

export type Tab = "log" | "dfg" | "model" | "conformance" | "resources";

/**
 * Opening tab for a freshly loaded analysis: inspect the raw event log first
 * when it is present, otherwise fall back to the directly-follows graph. Exports
 * from older tool versions carry no traces, so they must not land on a log tab
 * that is never rendered.
 */
export function initialTab(data: MineraExport): Tab {
  return data.traces && data.traces.length > 0 ? "log" : "dfg";
}

/** Whether any filter constraint is set; drives the "filtered" badge. */
export function hasActiveFilters(filters: FilterOptions): boolean {
  return Boolean(
    filters.topVariants ||
      filters.minVariantFrequency ||
      filters.removeActivities?.length ||
      filters.keepActivities?.length ||
      filters.startActivities?.length ||
      filters.endActivities?.length ||
      filters.minLength ||
      filters.maxLength,
  );
}

/**
 * Largest edge frequency in the log, floored at 1 so edge-weight scaling never
 * divides by zero on a log with no edges (e.g. a single-activity log).
 */
export function maxEdgeCount(data: MineraExport): number {
  return Math.max(1, ...data.edges.map((edge) => edge.count));
}

/** Whether an opened file is a pre-mined JSON export rather than a raw CSV log. */
export function isJsonExport(name: string): boolean {
  return /\.json$/i.test(name);
}

/**
 * Field delimiter implied by a file's name: a tab for ".tsv" uploads, otherwise
 * undefined so the backend applies its default (comma).
 */
export function delimiterForFile(name: string): string | undefined {
  return /\.tsv$/i.test(name) ? "\t" : undefined;
}
