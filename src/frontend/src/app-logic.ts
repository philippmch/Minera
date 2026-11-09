import {hasExactFilters} from "./exact-filter-controls";
import { type FilterOptions, type MinerOptions } from "./api";
import { type MineraExport } from "./types";
import { traceFacts } from "./mine-traces";
import {
  DECLARE_TEMPLATES,
  FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS,
} from "./types";

export type Tab = "log" | "dotted" | "timing" | "behavior" | "dfg" | "model" | "conformance" | "resources" | "embeddings" | "llm" | "query" | "anonymization" | "clustering" | "transformation";

/**
 * Opening tab for a freshly loaded analysis: inspect the raw event log first
 * when it is present, otherwise fall back to the directly-follows graph. Exports
 * from older tool versions carry no traces, so they must not land on a log tab
 * that is never rendered.
 */
export function initialTab(data: MineraExport): Tab {
  return traceFacts(data).hasCases ? "log" : "dfg";
}

/**
 * The view to render for the selected tab. A re-mine keeps the selected tab,
 * but the new analysis may no longer offer it (no traces, no timestamps, no
 * conformance); the first available view is shown instead, falling back to
 * the directly-follows graph.
 */
export function displayedTab(tab: Tab, available: readonly Tab[]): Tab {
  return available.includes(tab) ? tab : available[0] ?? "dfg";
}

/**
 * Whether any miner setting deviates from the backend defaults; drives the
 * same "active" badge as the filters. Zero is a meaningful threshold for the
 * heuristic settings, so for those only absence means default.
 */
export function hasActiveMinerOptions(options: MinerOptions): boolean {
  if (hasExactFilters(options)) return true;
  const allLogSkeletonConstraints = [
    "equivalence",
    "always_after",
    "always_before",
    "never_together",
    "directly_follows",
    "activ_freq",
  ];
  const logSkeletonConstraintsActive =
    options.logSkeletonConstraints !== undefined &&
    (options.logSkeletonConstraints.length === 0 ||
      options.logSkeletonConstraints.length !== allLogSkeletonConstraints.length ||
      allLogSkeletonConstraints.some(
        (name) => !options.logSkeletonConstraints?.includes(name),
      ));
  const declareAllowedTemplatesActive =
    options.declareAllowedTemplates !== undefined &&
    (options.declareAllowedTemplates.length === 0 ||
      options.declareAllowedTemplates.length !== DECLARE_TEMPLATES.length ||
      DECLARE_TEMPLATES.some(
        (name) => !options.declareAllowedTemplates?.includes(name),
      ));
  const footprintsDiscoveryActive =
    options.footprintsDiscoveryVariants !== undefined &&
    (options.footprintsDiscoveryVariants.length !== FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.length ||
      FOOTPRINTS_DEFAULT_DISCOVERY_VARIANTS.some(
        (name) => !options.footprintsDiscoveryVariants?.includes(name),
      ));
  const footprintsConformanceActive =
    options.footprintsConformanceVariants !== undefined &&
    (options.footprintsConformanceVariants.length !== 1 ||
      !options.footprintsConformanceVariants.includes("log_model"));
  const dfgStatisticalActive = options.dfgStatisticalVariants?.some((name) =>
    ["native", "frequency", "frequency_greedy", "frequency_triples", "performance",
      "performance_greedy", "case_attributes", "clean", "clean_polars", "clean_time"].includes(name),
  );
  const dfgCausalActive = options.dfgCausalVariants?.some((name) =>
    ["alpha", "heuristic"].includes(name),
  );
  const correlationActive = Boolean(options.correlationVariants?.some((name) =>
    ["classic", "classic_split", "trace_based"].includes(name),
  ));
  const spectrumActive = (options.spectrumActivities ?? []).filter((name) => name.trim()).length >= 2;
  return Boolean(
    options.noiseThreshold ||
      (options.inductiveVariant && options.inductiveVariant !== "log") ||
      options.inductiveDisableStrictSequenceCut === true ||
      options.inductiveDisableFallthroughs === true ||
      options.inductiveMultiprocessing === true ||
      options.inductiveSortEventsByTimestamp === true ||
      Boolean(options.inductiveActivityKey?.trim() &&
        options.inductiveActivityKey.trim() !== "concept:name") ||
      Boolean(options.inductiveTimestampKey?.trim() &&
        options.inductiveTimestampKey.trim() !== "time:timestamp") ||
      Boolean(options.inductiveCaseIdKey?.trim() &&
        options.inductiveCaseIdKey.trim() !== "case:concept:name") ||
      (options.alphaVariant && options.alphaVariant !== "classic") ||
      Boolean(options.alphaActivityAttribute?.trim() &&
        options.alphaActivityAttribute.trim() !== "concept:name") ||
      options.alphaRemoveUnconnected === true ||
      options.alphaMaxRowsToMaterialize !== undefined ||
      (options.heuristicVariant && options.heuristicVariant !== "classic") ||
      (options.heuristicActivityAttribute &&
        options.heuristicActivityAttribute.trim() !== "concept:name") ||
      (options.heuristicTimestampAttribute &&
        options.heuristicTimestampAttribute.trim() !== "time:timestamp") ||
      options.dependencyThreshold !== undefined ||
      options.andThreshold !== undefined ||
      options.minEdgeFrequency !== undefined ||
      options.heuristicMinActivityCount !== undefined ||
      (options.heuristicDecoration &&
        options.heuristicDecoration !== "frequency") ||
      (options.heuristicPerformanceAggregation &&
        options.heuristicPerformanceAggregation !== "mean") ||
      options.heuristicBusinessHours === true ||
      (options.heuristicDfgPreCleaningNoiseThreshold !== undefined &&
        options.heuristicDfgPreCleaningNoiseThreshold !== 0.05) ||
      (options.heuristicLoopLengthTwoThreshold !== undefined &&
        options.heuristicLoopLengthTwoThreshold !== 0.5) ||
      options.heuristicKeepOncePerCase === true ||
      Boolean(options.heuristicMostCommonPaths?.trim()) ||
      Boolean(options.heuristicStartTimestampAttribute?.trim()) ||
      (options.heuristicLifecycleInstanceAttribute &&
        options.heuristicLifecycleInstanceAttribute !== "concept:instance") ||
      (options.transitionView && options.transitionView !== "sequence") ||
      options.transitionDirection && options.transitionDirection !== "forward" ||
      options.transitionWindow !== undefined && options.transitionWindow !== 2 ||
      options.transitionIncludeData === true ||
      Boolean(options.transitionActivityKey?.trim() &&
        options.transitionActivityKey.trim() !== "concept:name") ||
      Boolean(options.transitionCaseIdKey?.trim() &&
        options.transitionCaseIdKey.trim() !== "case:concept:name") ||
      (options.powlVariant && options.powlVariant !== "maximal") ||
        (options.powlFilteringType && options.powlFilteringType !== "decreasing_factor") ||
      (options.powlFilteringThreshold !== undefined && options.powlFilteringThreshold !== 0) ||
        (options.powlOrderFrequencyRatio !== undefined && options.powlOrderFrequencyRatio !== 1) ||
        (options.powlBruteForceActivityLimit !== undefined && options.powlBruteForceActivityLimit !== 0) ||
        Boolean(options.powlActivityKey?.trim() &&
          options.powlActivityKey.trim() !== "concept:name") ||
        Boolean(options.powlTimestampKey?.trim() &&
          options.powlTimestampKey.trim() !== "time:timestamp") ||
        Boolean(options.powlCaseIdKey?.trim() &&
          options.powlCaseIdKey.trim() !== "case:concept:name") ||
      (options.tokenReplayVariant && options.tokenReplayVariant !== "forward") ||
      options.tokenReplayStopImmediatelyUnfit === true ||
      options.tokenReplayConsiderRemainingInFitness === false ||
      options.tokenReplayTryToReachFinalMarkingThroughHidden === false ||
      options.tokenReplayConsiderActivitiesNotInModelInFitness === true ||
      options.tokenReplayMaxRowsToMaterialize !== undefined ||
      Boolean(options.tokenReplayCaseIdAttribute?.trim() &&
        options.tokenReplayCaseIdAttribute.trim() !== "case:concept:name") ||
      options.tokenReplayCleaningTokenFlood === true ||
      options.tokenReplayExhaustiveInvisibleExploration === true ||
      options.tokenReplayWalkThroughHiddenTransitions === false ||
      options.tokenReplayIncludeTraceDiagnostics === true ||
      options.tokenReplayIncludeDurationDiagnostics === true ||
      options.tokenReplayIncludeRootCauseDiagnostics === true ||
      Boolean(options.tokenReplayRootCauseStringAttributes?.length) ||
      Boolean(options.tokenReplayRootCauseNumericAttributes?.length) ||
      options.tokenReplayRootCauseEnableMultiplier === true ||
      Boolean(options.tokenReplayTimestampAttribute?.trim() &&
        options.tokenReplayTimestampAttribute.trim() !== "time:timestamp") ||
      Boolean(options.tokenReplayActivityAttribute?.trim() &&
        options.tokenReplayActivityAttribute.trim() !== "concept:name") ||
      options.logSkeletonNoiseThreshold !== undefined &&
        options.logSkeletonNoiseThreshold !== 0 ||
      logSkeletonConstraintsActive ||
      options.declareConsideredActivities?.length ||
      options.declareConsideredActivitiesExact !== undefined ||
      options.dfgCaseAttributesExact !== undefined ||
      declareAllowedTemplatesActive ||
      options.declareMinSupportRatio !== undefined ||
      options.declareMinConfidenceRatio !== undefined ||
        (options.declareAutoSelectionMultiplier !== undefined &&
          options.declareAutoSelectionMultiplier !== 0.8) ||
      footprintsDiscoveryActive ||
      options.footprintsModel && options.footprintsModel !== "process_tree" ||
      footprintsConformanceActive ||
      options.footprintsStrict === true ||
      options.footprintsEnableActAlwaysExecuted === false ||
      Boolean(options.footprintsActivityAttribute?.trim() &&
        options.footprintsActivityAttribute.trim() !== "concept:name") ||
      Boolean(options.footprintsCaseIdAttribute?.trim() &&
        options.footprintsCaseIdAttribute.trim() !== "case:concept:name") ||
      Boolean(options.footprintsTimestampAttribute?.trim() &&
        options.footprintsTimestampAttribute.trim() !== "time:timestamp") ||
      Boolean(options.footprintsIndexAttribute?.trim() &&
        options.footprintsIndexAttribute.trim() !== "@@index") ||
      Boolean(options.footprintsStartTimestampAttribute?.trim()) ||
      options.footprintsSortRequired === false ||
      Boolean(options.footprintsInitialMarking?.trim()) ||
      dfgStatisticalActive || dfgCausalActive || correlationActive
      || Boolean(correlationActive && options.correlationActivityAttribute?.trim() &&
        options.correlationActivityAttribute.trim() !== "concept:name")
      || Boolean(correlationActive && options.correlationTimestampAttribute?.trim() &&
        options.correlationTimestampAttribute.trim() !== "time:timestamp")
      || Boolean(correlationActive && options.correlationStartTimestampAttribute?.trim())
      || Boolean(correlationActive && options.correlationCaseIdAttribute?.trim() &&
        options.correlationCaseIdAttribute.trim() !== "case:concept:name")
      || Boolean(options.temporalActivityAttribute?.trim() &&
        options.temporalActivityAttribute.trim() !== "concept:name")
      || Boolean(options.temporalCaseIdAttribute?.trim() &&
        options.temporalCaseIdAttribute.trim() !== "case:concept:name")
      || Boolean(options.temporalSourceTimestampAttribute?.trim())
      || Boolean(options.temporalTargetTimestampAttribute?.trim())
      || options.temporalBusinessHours === true
      || Boolean(options.temporalBusinessHourSlots?.trim())
      || options.temporalStrictAttributes === true
      || spectrumActive
      || (options.batches === true)
      || (options.minimumSelfDistanceVariant && options.minimumSelfDistanceVariant !== "log")
      || Boolean(options.minimumSelfDistanceActivityKey?.trim())
      || Boolean(options.minimumSelfDistanceCaseIdKey?.trim())
      || (options.overlap === true)
  );
}

/** Whether any filter constraint is set; drives the "filtered" badge. */
export function hasActiveFilters(filters: FilterOptions): boolean {
  if (hasExactFilters(filters)) return true;
  return Boolean(
    filters.topVariants ||
      filters.minVariantFrequency ||
      filters.variantCoverage ||
      filters.variantMinCoverage !== undefined ||
      filters.variantMaxCoverage !== undefined ||
      filters.variantPercentage !== undefined ||
      filters.keepDirectlyFollows?.length ||
      filters.removeDirectlyFollows?.length ||
      filters.keepEventuallyFollows?.length ||
      filters.removeEventuallyFollows?.length ||
      filters.tracePatterns?.length ||
      filters.admittedVariants?.length ||
      filters.prefixActivity ||
      filters.suffixActivity ||
      (filters.prefixActivity &&
        (filters.prefixStrict === false || filters.prefixOccurrence === "last")) ||
      (filters.suffixActivity &&
        (filters.suffixStrict === false || filters.suffixOccurrence === "last")) ||
      filters.segmentPrefixActivity ||
      filters.segmentSuffixActivity ||
      (filters.segmentPrefixActivity &&
        (filters.segmentPrefixStrict === false ||
          filters.segmentPrefixOccurrence === "last")) ||
      (filters.segmentSuffixActivity &&
        (filters.segmentSuffixStrict === false ||
          filters.segmentSuffixOccurrence === "last")) ||
      filters.segmentBetweenStarts?.length && filters.segmentBetweenEnds?.length ||
      (filters.pathPredecessor && filters.pathSuccessor) ||
      filters.maxCases !== undefined ||
      filters.removeActivities?.length ||
      filters.keepActivities?.length ||
      filters.removeResources?.length ||
      filters.keepResources?.length ||
      filters.removeLifecycles?.length ||
      filters.keepLifecycles?.length ||
      filters.startActivities?.length ||
      filters.endActivities?.length ||
      (filters.startActivities?.length && filters.startActivitiesPositive === false) ||
      (filters.endActivities?.length && filters.endActivitiesPositive === false) ||
      filters.minLength ||
      filters.maxLength ||
      filters.timeFrom ||
      filters.timeTo ||
      filters.caseDurationMin !== undefined ||
      filters.caseDurationMax !== undefined ||
      filters.maxActivities !== undefined ||
      filters.relativeAttributeKey !== undefined ||
      filters.relativeAttributeStake !== undefined ||
      filters.relativeAttributeKeepOncePerCase === false ||
      filters.attributeOccurrenceMin !== undefined ||
      (filters.numericAttributeKey &&
        (filters.numericAttributeMin !== undefined ||
          filters.numericAttributeMax !== undefined)) ||
      filters.reworkActivity ||
      filters.attributeKey ||
      filters.caseAttributeKey ||
      filters.ltlVariant && filters.ltlAttributeValues?.length ||
      (filters.betweenStart && filters.betweenEnd) ||
      filters.activitySplit?.length ||
      filters.repetitionValue !== undefined ||
      filters.consecutiveGrouping ||
      filters.timestampGrouping,
  );
}

/**
 * Distinct lifecycle transitions the log's events carry, in first-seen order.
 * Empty for a log of atomic events; feeds the lifecycle filter chips.
 */
export function logLifecycles(data: MineraExport): string[] {
  // From the traces when the analysis carries them, else from its trace summary.
  return [...traceFacts(data).lifecycles];
}

/**
 * Largest edge frequency in the log, floored at 1 so edge-weight scaling never
 * divides by zero on a log with no edges (e.g. a single-activity log).
 */
export function maxEdgeCount(data: MineraExport): number {
  // A loop, not Math.max(...counts): spreading a very large edge list overflows the call stack.
  let max = 1;
  for (const edge of data.edges) if (edge.count > max) max = edge.count;
  return max;
}

/** Whether an opened file is JSON (a pre-mined export, OCEL or EventTable) rather than a raw CSV log. */
export function isJsonExport(name: string): boolean {
  return /\.(json|jsonocel)$/i.test(name);
}

/** Whether a JSON document carries either supported object-centric envelope. */
export function isOcelJsonDocument(value: unknown): boolean {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  const hasPair = (events: string, objects: string) =>
    record[events] !== null && typeof record[events] === "object" &&
    record[objects] !== null && typeof record[objects] === "object";
  return hasPair("ocel:events", "ocel:objects") ||
    hasPair("events", "objects");
}

/**
 * Field delimiter implied by a file's name: a tab for ".tsv" uploads, otherwise
 * undefined so the backend applies its default (comma).
 */
export function delimiterForFile(name: string): string | undefined {
  return /\.tsv$/i.test(name) ? "\t" : undefined;
}

/** Delimiters a CSV header line is checked for, in tie-break order. */
export const CSV_HEADER_DELIMITERS = [",", ";", "\t", "|"] as const;

/** Whether an uploaded file is mined through the CSV/TSV reader. */
export const isDelimitedTextName = (name: string): boolean => /\.(?:csv|tsv)$/i.test(name);

/**
 * The records of a CSV text, as pandas' C parser splits them: line breaks end a record outside double quotes, a quote
 * opens a quoted field only at the start of a field (the record start or after a candidate delimiter; `12" screen`
 * keeps its quote), `""` inside quotes is a literal quote, a BOM is dropped and blank lines (only spaces or tabs) are
 * skipped. At most `limit` records; when the text is the start of a longer file (`truncated`), a last record the text
 * may have cut short is left out.
 */
function csvRecords(text: string, truncated: boolean, limit: number): string[] {
  const body = text.charCodeAt(0) === 0xfeff ? text.slice(1) : text, records: string[] = [];
  const delimiters: readonly string[] = CSV_HEADER_DELIMITERS;
  const add = (record: string) => { if (!/^[ \t]*$/.test(record)) records.push(record); };
  let quoted = false, start = 0;
  for (let i = 0; i <= body.length && records.length < limit; i += 1) {
    const c = body[i];
    if (quoted) {
      if (i === body.length) { if (!truncated) add(body.slice(start, i)); break; }
      if (c === '"') { if (body[i + 1] === '"') i += 1; else quoted = false; }
      continue;
    }
    if (c === '"' && (i === start || delimiters.includes(body[i - 1]))) { quoted = true; continue; }
    if (i === body.length || c === "\n" || c === "\r") {
      if (i === body.length && truncated) break;
      add(body.slice(start, i));
      if (c === "\r" && body[i + 1] === "\n") i += 1;
      start = i + 1;
    }
  }
  return records;
}

/** The header line of a CSV text: its first record, after any blank lines (pandas skips them), without a BOM. */
export function csvHeaderLine(text: string): string {
  return csvRecords(text, false, 1)[0] ?? "";
}

/** Splits a header line on `delimiter`, honoring double quotes that open a field ("" is a literal quote). */
export function csvHeaderNames(line: string, delimiter: string): string[] {
  const names: string[] = [];
  let current = "", quoted = false, fieldStart = true;
  for (let i = 0; i < line.length; i += 1) {
    const c = line[i];
    if (quoted) {
      if (c === '"' && line[i + 1] === '"') { current += '"'; i += 1; }
      else if (c === '"') quoted = false;
      else current += c;
    } else if (c === '"' && fieldStart) { quoted = true; fieldStart = false; }
    else if (c === delimiter) { names.push(current); current = ""; fieldStart = true; }
    else { current += c; fieldStart = false; }
  }
  names.push(current);
  return names;
}

/**
 * The data records of a CSV sample after its header line (split as csvRecords does: blank lines skipped, quotes only
 * at field starts), at most `limit` of them. When the sample is the start of a longer file (`truncated`), its last
 * record may be cut short and is left out.
 */
export function csvDataRows(text: string, truncated: boolean, limit = 20): string[] {
  return csvRecords(text, truncated, limit + 1).slice(1);
}

/**
 * Field delimiter of a CSV or TSV upload. The name gives the default (tab for ".tsv", otherwise comma, which maps to
 * undefined so the backend applies its own default), and a ".tsv" header holding a tab always keeps the tab. Another
 * candidate (comma, semicolon, tab, pipe) replaces the default only when the default leaves the header a single column,
 * or when every sampled data row has the candidate's header field count and the rows either do not all have the
 * default's or the candidate splits the header into more fields. Among replacements the most fields win, ties going to
 * the earlier candidate; a header whose names hold a stray comma or semicolon keeps the file's delimiter.
 */
export function delimiterForHeader(name: string, header: string, rows: readonly string[] = []): string | undefined {
  const fallback = delimiterForFile(name) ?? ",";
  const fields = (line: string, delimiter: string) => csvHeaderNames(line, delimiter).length;
  const result = (delimiter: string) => delimiter === "," ? undefined : delimiter;
  const own = fields(header, fallback);
  if (fallback === "\t" && own > 1) return "\t";
  const consistent = (delimiter: string) => rows.every(row => fields(row, delimiter) === fields(header, delimiter));
  const ownConsistent = consistent(fallback);
  let best: string | undefined, bestCount = 1;
  for (const delimiter of CSV_HEADER_DELIMITERS) {
    if (delimiter === fallback) continue;
    const count = fields(header, delimiter);
    if (count <= bestCount) continue;
    if (own === 1 || (rows.length > 0 && consistent(delimiter) && (!ownConsistent || count > own))) { best = delimiter; bestCount = count; }
  }
  return result(best ?? fallback);
}

/** A column-not-found message with the header names that were read, so a mismatch is visible. */
export function columnNotFoundMessage(message: string, header: string | null, delimiter: string | undefined): string {
  if (header === null || !/^CSV column not found:/.test(message)) return message;
  const names = csvHeaderNames(header, delimiter ?? ",").map(name => JSON.stringify(name)).join(", ");
  return `${message} (the header has ${names}). Choose the columns in Source inspector.`;
}

/** Every Petri model that the quality evaluators can address in this view. */
export function evaluationModelIds(data: MineraExport, importedAvailable = false): string[] {
  const ids = new Set<string>([
    ...Object.keys(data.conformance ?? {}),
    ...Object.keys(data.evaluation ?? {}),
  ]);
  if (data.inductive_petri_net) ids.add("inductive");
  if (data.petri_net) ids.add("alpha");
  if (data.heuristic_petri_net) ids.add("heuristic");
  if (data.powl_petri_net) ids.add("powl");
  if (importedAvailable) ids.add("imported");
  return [...ids].sort();
}

/** How a rollback (cancel-mine) settled; undefined when nothing was posted, so the backend never left the previous source. */
export type LiveSourceRollback = {confirmed: boolean; canceled?: boolean; activeSourceId?: string | null} | undefined | void;

/**
 * Hands the previous live backend source back after a load that failed or was
 * cancelled. The backend keeps (or, after cancel-mine, restores) its previous
 * session in that case, so the still-displayed analysis is live again once the
 * rollback request has finished, but only when the backend confirmed the
 * cancel and, if it reported the source it now holds, that source is the
 * previous one: a cancel that failed, or one overtaken by another load, leaves
 * the analysis offline instead of claiming a source the backend no longer
 * holds. A newer load, or going home, supersedes a pending handback.
 */
export class LiveSourceHandback {
  private epoch = 0;
  /** A new load or going home: any pending handback must not apply. */
  supersede(): void { this.epoch += 1; }
  async handback(previous: string | null | undefined, rollback: Promise<LiveSourceRollback>, restore: (sourceId: string) => void): Promise<boolean> {
    if (!previous) return false;
    const epoch = ++this.epoch;
    const outcome: LiveSourceRollback = await rollback.catch(() => ({confirmed: false}));
    if (epoch !== this.epoch) return false;
    // A cancel that changed nothing (the request had failed, or its result was applied for good) restores only when the
    // backend reports still holding the previous source.
    if (outcome && (!outcome.confirmed || (outcome.activeSourceId !== undefined && outcome.activeSourceId !== previous) ||
                    (outcome.canceled === false && outcome.activeSourceId === undefined))) return false;
    restore(previous);
    return true;
  }
}

/** A workspace run stays valid when another model is imported, unless it ran on the imported model itself. */
export function runSurvivesModelImport(runMiner: string | null | undefined): boolean {
  return runMiner !== "imported";
}

/** Whether a run started on `miner` at imported-model generation `started` is stale now (generation `current`): another model was imported while a run on the imported net was in flight. */
export function importedRunStale(miner: string | null | undefined, started: number, current: number): boolean {
  return started !== current && !runSurvivesModelImport(miner);
}

export const IMPORTED_MODEL_CHANGED = "The imported model changed while this run was pending; run it again.";

/** Case rows with stable React keys: logs may repeat a case id, so rows are keyed by trace position. */
const caseIdCollator = new Intl.Collator("en", { numeric: true });
const plainNumber = /^[+-]?(?:\d+\.?\d*|\.\d+)$/;
/** Natural case-id order: two plain numbers by value (-10 before -3), anything else by numeric-aware collation. */
export function compareCaseIds(a: string, b: string): number {
  if (plainNumber.test(a) && plainNumber.test(b)) { const d = Number(a) - Number(b); if (d !== 0) return d < 0 ? -1 : 1; }
  return caseIdCollator.compare(a, b);
}
/** The event-log case list in natural case-id order (C2 before C10); equal ids keep the log's order. Display only:
 * views that mirror a reference output keep the backend's order. */
export function caseRowEntries<T extends { case_id: string }>(traces: readonly T[], query: string): Array<{ key: number; trace: T }> {
  const needle = query.trim().toLowerCase(), rows: Array<{ key: number; trace: T }> = [];
  traces.forEach((trace, key) => { if (!needle || trace.case_id.toLowerCase().includes(needle)) rows.push({ key, trace }); });
  return rows.sort((a, b) => compareCaseIds(a.trace.case_id, b.trace.case_id) || a.key - b.key);
}

const PYTHON_FLOAT_DECIMAL = /^[+-]?(?:(?:\d(?:_?\d)*)?\.\d(?:_?\d)*|\d(?:_?\d)*\.?)(?:[eE][+-]?\d(?:_?\d)*)?$/;
const PYTHON_FLOAT_SPECIAL = /^([+-]?)(?:(inf|infinity)|nan)$/i;
/** Python's (Unicode 14) whitespace beyond ASCII, which float() reads as a space. */
const PYTHON_UNICODE_SPACES = new Set([0x85, 0xa0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000]);
/** The zero of every non-ASCII run of decimal digits (Unicode 14, as Python and the backend parser classify them). */
const PYTHON_UNICODE_DIGIT_ZEROS = [0x660, 0x6f0, 0x7c0, 0x966, 0x9e6, 0xa66, 0xae6, 0xb66, 0xbe6, 0xc66, 0xce6, 0xd66, 0xde6, 0xe50, 0xed0, 0xf20, 0x1040, 0x1090, 0x17e0, 0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90, 0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620, 0xa8d0, 0xa900, 0xa9d0, 0xa9f0, 0xaa50, 0xabf0, 0xff10, 0x104a0, 0x10d30, 0x11066, 0x110f0, 0x11136, 0x111d0, 0x112f0, 0x11450, 0x114d0, 0x11650, 0x116c0, 0x11730, 0x118e0, 0x11950, 0x11c50, 0x11d50, 0x11da0, 0x16a60, 0x16ac0, 0x16b50, 0x1d7ce, 0x1d7d8, 0x1d7e2, 0x1d7ec, 0x1d7f6, 0x1e140, 0x1e2f0, 0x1e950, 0x1fbf0];
/**
 * Python's float(str) over a whole text, as the backend reads ratio controls: non-ASCII Unicode whitespace and decimal
 * digits read as a space and ASCII digits first; then surrounding ASCII whitespace (space, \t, \n, \v, \f, \r), an
 * optional sign, underscores only between digits, a decimal mantissa with an optional complete exponent, or
 * inf/infinity/nan in any case. Hexadecimal, a lone ".", an exponent without digits, other characters (U+FEFF and the
 * ASCII separators U+001C–U+001F included) and trailing text are refused (null).
 */
export function parsePythonFloat(text: string): number | null {
  let ascii = "";
  for (const character of text) {
    const code = character.codePointAt(0)!;
    if (code < 0x80) { ascii += character; continue; }
    if (PYTHON_UNICODE_SPACES.has(code)) { ascii += " "; continue; }
    const zero = PYTHON_UNICODE_DIGIT_ZEROS.find(start => code >= start && code < start + 10);
    if (zero === undefined) return null;
    ascii += String(code - zero);
  }
  const trimmed = ascii.replace(/^[ \t\n\v\f\r]+|[ \t\n\v\f\r]+$/g, "");
  if (PYTHON_FLOAT_DECIMAL.test(trimmed)) return Number(trimmed.replace(/_/g, ""));
  const special = PYTHON_FLOAT_SPECIAL.exec(trimmed);
  if (!special) return null;
  return special[2] ? (special[1] === "-" ? -Infinity : Infinity) : NaN;
}

/** A numeric input's value: an emptied field means "not set" rather than 0. */
export function optionalNumber(text: string): number | undefined {
  return text.trim() === "" ? undefined : Number(text);
}

const identityKeys = new WeakMap<object, number>();
let nextIdentityKey = 0;
/** A React key for an object's identity, e.g. a File: two uploads with the same name stay distinct. */
export function identityKey(value: object | null | undefined): string {
  if (!value) return "none";
  let key = identityKeys.get(value);
  if (key === undefined) { key = ++nextIdentityKey; identityKeys.set(value, key); }
  return `object-${key}`;
}

/** "1 case", "2 cases", "1,234 events": a formatted count with the noun in the right number. */
export function countLabel(count: number, singular: string, plural = `${singular}s`): string {
  return `${new Intl.NumberFormat("en-US").format(count)} ${count === 1 ? singular : plural}`;
}

/** Whether an object-centric discovery result was computed from a log with no objects (nothing to show). */
export function ocelResultHasNoObjects(result: Record<string, unknown> | null | undefined): boolean {
  if (!result) return false;
  const types = result.object_types;
  return Array.isArray(types) && types.length === 0;
}
