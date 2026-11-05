import {
  formatDuration,
  numberFormat,
  type PetriDecoration,
} from "./types";

// Pure label logic for the Petri net's frequency/performance overlay, kept
// out of the component so it can be unit-tested.

/**
 * Badge for a transition's firing count. Every decorated transition gets one
 * — an explicit "0" marks dead paths the log never took, which is exactly
 * what a frequency overlay should expose.
 */
export function transitionCountLabel(
  decoration: PetriDecoration,
  transition: number,
): string {
  return numberFormat.format(decoration.transitions[transition] ?? 0);
}

/**
 * Mean-wait label for a place, or null when no timed token ever rested there
 * (an unlabeled place reads as "no data", matching the process map).
 */
export function placeWaitLabel(
  decoration: PetriDecoration,
  place: number,
): string | null {
  const wait = decoration.places[place]?.wait;
  if (!wait || wait.count === 0) return null;
  return formatDuration(wait.mean);
}

/** Tooltip for a place: tokens through it, plus wait stats when timed. */
export function placeTooltip(
  decoration: PetriDecoration,
  place: number,
): string | null {
  const entry = decoration.places[place];
  if (!entry) return null;
  const tokens = `${numberFormat.format(entry.count)} token${
    entry.count === 1 ? "" : "s"
  }`;
  if (entry.wait.count === 0) return tokens;
  return `${tokens} · waits ${formatDuration(entry.wait.median)} median · ${formatDuration(entry.wait.mean)} mean · up to ${formatDuration(entry.wait.max)}`;
}

/** Whether the performance mode has anything to show (some place was timed). */
export function hasTimedDecoration(
  decoration: PetriDecoration | null | undefined,
): boolean {
  return Boolean(decoration?.places.some((place) => place.wait.count > 0));
}

/** The "replayed n of m cases" caption; null when nothing replayed cleanly. */
export function decorationCoverage(
  decoration: PetriDecoration,
): string | null {
  if (decoration.decorated_traces === 0) return null;
  if (decoration.decorated_traces === decoration.trace_count) {
    return `replayed all ${numberFormat.format(decoration.trace_count)} cases`;
  }
  return `replayed ${numberFormat.format(decoration.decorated_traces)} of ${numberFormat.format(decoration.trace_count)} cases`;
}

/** The server's note on a decoration that leaves cases out; null when none. */
export function decorationNotice(
  decoration: PetriDecoration | null | undefined,
): string | null {
  return decoration?.message ? decoration.message : null;
}
