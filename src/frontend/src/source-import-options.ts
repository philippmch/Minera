import { type MineOptions } from "./api";

// How the original upload is read: CSV columns and delimiter, XES importer
// settings and the native mining-source selectors. Everything else in a
// source's options (filters, miner settings, exact controls) is replaced by
// each Apply or Reset rather than carried over from the previous mine.
const SOURCE_IMPORT_KEYS = [
  "caseColumn", "activityColumn", "timestampColumn", "delimiter",
  "xesVariant", "xesCaseAttribute", "xesActivityAttribute", "xesTimestampAttribute",
  "xesResourceAttribute", "xesLifecycleAttribute", "xesRequireTimestamp",
  "xesTimestampSort", "xesReverseSort", "xesMaxTraces", "xesAttributesToRead",
  "xesMaxDecompressedBytes",
  "miningInputFormat", "miningInputShape", "miningSelectors", "miningImportFields",
] as const satisfies ReadonlyArray<keyof MineOptions>;

export function sourceImportOptions(options: MineOptions): MineOptions {
  const result: MineOptions = {};
  for (const key of SOURCE_IMPORT_KEYS)
    if (options[key] !== undefined) Object.assign(result, {[key]: options[key]});
  return result;
}
