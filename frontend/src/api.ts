import {
  type ActivityStat,
  type Alignments,
  type MineraExport,
  type ModelConformance,
  isMineraExport,
} from "./types";

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

/** The backend's answer to checking an uploaded PNML model against the log. */
export interface ImportedModelCheck {
  /**
   * The activity registry, re-sent because the imported model may name
   * activities the log never shows; ids in `model` index into this list.
   */
  activities: ActivityStat[];
  model: ModelConformance;
}

/**
 * Checks the most recently mined log against a user-supplied PNML workflow
 * net. The server keeps the net under the miner id "imported", so alignments
 * can be requested for it afterwards like for any discovered model.
 */
export async function checkModel(file: File): Promise<ImportedModelCheck> {
  const form = new FormData();
  form.append("model", file, file.name);
  let response: Response;
  try {
    response = await fetch("/api/check-model", { method: "POST", body: form });
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
  if (
    parsed === null ||
    typeof parsed !== "object" ||
    !("model" in parsed) ||
    !("activities" in parsed)
  ) {
    throw new Error("the Minera backend returned an invalid model check");
  }
  return parsed as ImportedModelCheck;
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
 * Fetches the most recently mined event log — after any filtering applied at
 * mine time — as standard XES. Like PNML, this is served from the backend's
 * cached session, so it only works for a log mined this session.
 */
export async function fetchXes(): Promise<string> {
  let response: Response;
  try {
    response = await fetch("/api/xes");
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
 * Simulates one miner's model into a synthetic event log (100 cases), served
 * as standard XES. On a sound model every generated trace replays perfectly,
 * so the playout doubles as a self-check and a shareable test log. Like the
 * other model endpoints this only works for a log mined this session.
 */
export async function fetchPlayout(miner: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(`/api/playout?miner=${encodeURIComponent(miner)}`);
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
 * Fetches one miner's model from the most recently mined log as BPMN 2.0.
 * The inductive model converts from its block-structured process tree; the
 * others convert from their workflow nets server-side. Like PNML this only
 * works for a log mined this session.
 */
export async function fetchBpmn(miner: string): Promise<string> {
  let response: Response;
  try {
    response = await fetch(`/api/bpmn?miner=${encodeURIComponent(miner)}`);
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
