import { type MineraExport, isMineraExport } from "./types";

export interface CsvOptions {
  caseColumn?: string;
  activityColumn?: string;
  timestampColumn?: string;
  delimiter?: string;
}

/** Uploads a CSV event log to the local Minera backend for mining. */
export async function mineCsv(
  file: File,
  options: CsvOptions = {},
): Promise<MineraExport> {
  const form = new FormData();
  form.append("log", file, file.name);
  if (options.caseColumn) form.append("case_column", options.caseColumn);
  if (options.activityColumn) form.append("activity_column", options.activityColumn);
  if (options.timestampColumn) form.append("timestamp_column", options.timestampColumn);
  if (options.delimiter) form.append("delimiter", options.delimiter);

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
