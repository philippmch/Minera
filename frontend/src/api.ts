import { type MineraExport, isMineraExport } from "./types";

export interface CsvOptions {
  caseColumn?: string;
  activityColumn?: string;
  timestampColumn?: string;
  delimiter?: string;
}

/** Log-preprocessing filters applied by the backend before discovery. */
export interface FilterOptions {
  topVariants?: number;
  minVariantFrequency?: number;
  keepActivities?: string[];
  removeActivities?: string[];
  startActivities?: string[];
  endActivities?: string[];
  minLength?: number;
  maxLength?: number;
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
  if (options.startActivities?.length)
    form.append("start_activities", options.startActivities.join(","));
  if (options.endActivities?.length)
    form.append("end_activities", options.endActivities.join(","));
  if (options.minLength) form.append("min_length", String(options.minLength));
  if (options.maxLength) form.append("max_length", String(options.maxLength));

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
