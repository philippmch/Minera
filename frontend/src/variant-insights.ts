import type { VariantStat } from "./types";

/**
 * Cumulative share of cases covered by the variants taken in order, as a value
 * in [0, 1] per index: entry `i` is the fraction of all cases explained by
 * variants `0..i` inclusive. The backend already emits variants most-frequent
 * first, so this reads as the classic "the top N variants cover X%" curve.
 *
 * `traceCount` is the total number of cases. When it is zero (or negative) every
 * entry is 0 so callers can render a flat, meaningless curve without dividing by
 * zero.
 */
export function cumulativeCoverage(
  variants: VariantStat[],
  traceCount: number,
): number[] {
  if (traceCount <= 0) return variants.map(() => 0);
  let running = 0;
  return variants.map((variant) => {
    running += variant.count;
    return running / traceCount;
  });
}
