import { type Alignments, type MineraExport, isMineraExport } from "./types";

export interface CsvOptions {
  caseColumn?: string;
  activityColumn?: string;
  timestampColumn?: string;
  delimiter?: string;
}

/** How the backend's time-range filter reads a trace. */
export type TimeFilterMode =
  | "events"
  | "traces_contained"
  | "traces_intersecting";

/** Log-preprocessing filters applied by the backend before discovery. */
export interface FilterOptions {
  topVariants?: number;
  minVariantFrequency?: number;
  keepActivities?: string[];
  removeActivities?: string[];
  keepResources?: string[];
  removeResources?: string[];
  startActivities?: string[];
  endActivities?: string[];
  /** Keep only cases where this activity occurs at least reworkMin times. */
  reworkActivity?: string;
  reworkMin?: number;
  /** Cut each case into the subtraces from betweenStart to betweenEnd. */
  betweenStart?: string;
  betweenEnd?: string;
  minLength?: number;
  maxLength?: number;
  /** ISO-8601 bounds, both inclusive; either side may be open. */
  timeFrom?: string;
  timeTo?: string;
  timeMode?: TimeFilterMode;
}

/** Uploads a CSV event log to the local Minera backend for mining. */
export async function mineCsv(
  file: File,
  options: CsvOptions & FilterOptions = {},
): Promise<MineraExport> {
  const form = new FormData();
  form.append("log", file, file.name);
  if (options.caseColumn) form.append("case_column", options.caseColumn);
  if (options.activityColumn) form.append("activity_column", options.activityColumn);
  if (options.timestampColumn) form.append("timestamp_column", options.timestampColumn);
  if (options.delimiter) form.append("delimiter", options.delimiter);

  // Filters. Activity lists are comma-joined to match the backend's parser.
  if (options.topVariants) form.append("top_variants", String(options.topVariants));
  if (options.minVariantFrequency)
    form.append("min_variant_frequency", String(options.minVariantFrequency));
  if (options.keepActivities?.length)
    form.append("keep_activities", options.keepActivities.join(","));
  if (options.removeActivities?.length)
    form.append("remove_activities", options.removeActivities.join(","));
  if (options.keepResources?.length)
    form.append("keep_resources", options.keepResources.join(","));
  if (options.removeResources?.length)
    form.append("remove_resources", options.removeResources.join(","));
  if (options.startActivities?.length)
    form.append("start_activities", options.startActivities.join(","));
  if (options.endActivities?.length)
    form.append("end_activities", options.endActivities.join(","));
  if (options.reworkActivity) {
    form.append("rework_activity", options.reworkActivity);
    if (options.reworkMin)
      form.append("rework_min_occurrences", String(options.reworkMin));
  }
  if (options.betweenStart && options.betweenEnd) {
    form.append("between_start", options.betweenStart);
    form.append("between_end", options.betweenEnd);
  }
  if (options.minLength) form.append("min_length", String(options.minLength));
  if (options.maxLength) form.append("max_length", String(options.maxLength));
  if (options.timeFrom) form.append("time_from", options.timeFrom);
  if (options.timeTo) form.append("time_to", options.timeTo);
  if (options.timeMode && (options.timeFrom || options.timeTo))
    form.append("time_mode", options.timeMode);

  let response: Response;
  try {
    response = await fetch("/api/mine", { method: "POST", body: form });
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (!isMineraExport(parsed)) {
    throw new Error("the Minera backend returned an invalid analysis");
  }
  return parsed;
}

/**
 * Asks the backend to align the most recently mined log against one miner's
 * model. The server defers this costlier computation to here so a mine never
 * pays for alignments the user may not open.
 */
export async function alignModel(miner: string): Promise<Alignments> {
  let response: Response;
  try {
    response = await fetch(`/api/align?miner=${encodeURIComponent(miner)}`, {
      method: "POST",
    });
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }

  const parsed: unknown = await response.json().catch(() => null);
  if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
    throw new Error(String((parsed as { error: unknown }).error));
  }
  if (!response.ok) {
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  if (parsed === null || typeof parsed !== "object" || !("computed" in parsed)) {
    throw new Error("the Minera backend returned an invalid alignment");
  }
  return parsed as Alignments;
}

/**
 * Fetches one miner's model from the most recently mined log as standard PNML.
 * The net is translated server-side (the single source of truth used for
 * conformance), so this only works for a log mined this session.
 */
export async function fetchPnml(miner: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(`/api/pnml?miner=${encodeURIComponent(miner)}`);
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}

/**
 * Fetches the inductive model from the most recently mined log as BPMN 2.0.
 * The block-structured process tree maps cleanly onto BPMN, so the backend
 * always serves that model; like PNML it only works for a log mined this
 * session.
 */
export async function fetchBpmn(): Promise<string> {
  let response: Response;
  try {
    response = await fetch("/api/bpmn");
  } catch {
    throw new Error(
      "could not reach the Minera backend — is minera_server running?",
    );
  }
  if (!response.ok) {
    const parsed: unknown = await response.json().catch(() => null);
    if (parsed !== null && typeof parsed === "object" && "error" in parsed) {
      throw new Error(String((parsed as { error: unknown }).error));
    }
    throw new Error(`the Minera backend answered with status ${response.status}`);
  }
  return response.text();
}
