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

/** How the backend's attribute filter applies: to events or to whole cases. */
export type AttributeFilterMode = "events" | "traces";

/** Log-preprocessing filters applied by the backend before discovery. */
export interface FilterOptions {
  topVariants?: number;
  minVariantFrequency?: number;
  keepActivities?: string[];
  removeActivities?: string[];
  keepResources?: string[];
  removeResources?: string[];
  /** Lifecycle projection: e.g. keepLifecycles: ["complete"] reduces a
   * start/complete log to atomic events for discovery. */
  keepLifecycles?: string[];
  removeLifecycles?: string[];
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
  /** Generic event-attribute filter: the key, its value constraints, and
   * whether it filters events or whole cases. */
  attributeKey?: string;
  keepAttributeValues?: string[];
  removeAttributeValues?: string[];
  attributeMode?: AttributeFilterMode;
  /** Case-attribute filter: the key and its value constraints; always
   * keeps or drops whole cases. */
  caseAttributeKey?: string;
  keepCaseAttributeValues?: string[];
  removeCaseAttributeValues?: string[];
}

/** Discovery options for the miners themselves, applied at mine time. */
export interface MinerOptions {
  /** Inductive Miner – infrequent noise threshold in [0, 1]; 0 is classic. */
  noiseThreshold?: number;
  /** "log" (default) splits sublogs; "dfg" recurses on the graph (IMd). */
  inductiveVariant?: "log" | "dfg";
  /** "plus" (default) handles short loops; "classic" is the textbook Alpha. */
  alphaVariant?: "plus" | "classic";
  /** Heuristic Miner: minimum dependency measure for an edge (default 0.9). */
  dependencyThreshold?: number;
  /** Heuristic Miner: minimum AND measure for a parallel split (default 0.65). */
  andThreshold?: number;
  /** Heuristic Miner: direct successions an edge needs (default 1). */
  minEdgeFrequency?: number;
}

export type MineOptions = CsvOptions & FilterOptions & MinerOptions;

/**
 * The form fields a mine request sends for the given options, as name/value
 * pairs. Pure so the option-to-field mapping is unit-testable; absent or
 * default-signalling options produce no field, leaving the backend defaults
 * in charge. Activity lists are comma-joined to match the backend's parser.
 */
export function mineFormEntries(options: MineOptions): Array<[string, string]> {
  const entries: Array<[string, string]> = [];
  const add = (name: string, value: string) => entries.push([name, value]);

  if (options.caseColumn) add("case_column", options.caseColumn);
  if (options.activityColumn) add("activity_column", options.activityColumn);
  if (options.timestampColumn) add("timestamp_column", options.timestampColumn);
  if (options.delimiter) add("delimiter", options.delimiter);

  if (options.topVariants) add("top_variants", String(options.topVariants));
  if (options.minVariantFrequency)
    add("min_variant_frequency", String(options.minVariantFrequency));
  if (options.keepActivities?.length)
    add("keep_activities", options.keepActivities.join(","));
  if (options.removeActivities?.length)
    add("remove_activities", options.removeActivities.join(","));
  if (options.keepResources?.length)
    add("keep_resources", options.keepResources.join(","));
  if (options.removeResources?.length)
    add("remove_resources", options.removeResources.join(","));
  if (options.keepLifecycles?.length)
    add("keep_lifecycles", options.keepLifecycles.join(","));
  if (options.removeLifecycles?.length)
    add("remove_lifecycles", options.removeLifecycles.join(","));
  if (options.startActivities?.length)
    add("start_activities", options.startActivities.join(","));
  if (options.endActivities?.length)
    add("end_activities", options.endActivities.join(","));
  if (options.reworkActivity) {
    add("rework_activity", options.reworkActivity);
    if (options.reworkMin)
      add("rework_min_occurrences", String(options.reworkMin));
  }
  if (options.betweenStart && options.betweenEnd) {
    add("between_start", options.betweenStart);
    add("between_end", options.betweenEnd);
  }
  if (options.minLength) add("min_length", String(options.minLength));
  if (options.maxLength) add("max_length", String(options.maxLength));
  if (options.timeFrom) add("time_from", options.timeFrom);
  if (options.timeTo) add("time_to", options.timeTo);
  if (options.timeMode && (options.timeFrom || options.timeTo))
    add("time_mode", options.timeMode);
  if (options.attributeKey) {
    add("attribute_key", options.attributeKey);
    if (options.keepAttributeValues?.length)
      add("keep_attribute_values", options.keepAttributeValues.join(","));
    if (options.removeAttributeValues?.length)
      add("remove_attribute_values", options.removeAttributeValues.join(","));
    if (options.attributeMode)
      add("attribute_mode", options.attributeMode);
  }
  if (options.caseAttributeKey) {
    add("case_attribute_key", options.caseAttributeKey);
    if (options.keepCaseAttributeValues?.length)
      add("keep_case_attribute_values", options.keepCaseAttributeValues.join(","));
    if (options.removeCaseAttributeValues?.length)
      add(
        "remove_case_attribute_values",
        options.removeCaseAttributeValues.join(","),
      );
  }

  // Miner settings. Zero is a meaningful heuristic threshold (unlike the
  // filters above, where zero means "off"), so these send whenever set.
  if (options.noiseThreshold !== undefined)
    add("noise_threshold", String(options.noiseThreshold));
  if (options.inductiveVariant) add("inductive_variant", options.inductiveVariant);
  if (options.alphaVariant) add("alpha_variant", options.alphaVariant);
  if (options.dependencyThreshold !== undefined)
    add("dependency_threshold", String(options.dependencyThreshold));
  if (options.andThreshold !== undefined)
    add("and_threshold", String(options.andThreshold));
  if (options.minEdgeFrequency !== undefined)
    add("min_edge_frequency", String(options.minEdgeFrequency));

  return entries;
}

/** Uploads a CSV event log to the local Minera backend for mining. */
export async function mineCsv(
  file: File,
  options: MineOptions = {},
): Promise<MineraExport> {
  const form = new FormData();
  form.append("log", file, file.name);
  for (const [name, value] of mineFormEntries(options)) form.append(name, value);

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
