// Registry of conformance-checking methods shown in the "Conformance" tab.
// Mirrors miners.ts: new methods can be added without touching the chooser UI
// by appending to CONFORMANCE_METHODS, and — only for a genuinely new result
// shape — teaching Conformance how to render that artifact.

import { type ActivityDeviation, type DecisionTreeNode } from "./types";

export type ConformanceArtifact =
  | "token-replay"
  | "alignments"
  | "footprints"
  | "decisions";

export interface ConformanceMethod {
  id: string;
  name: string;
  /** One-line summary shown on the method-selection card. */
  tagline: string;
  description: string;
  /** Which result representation this method produces. */
  artifact: ConformanceArtifact;
}

export const CONFORMANCE_METHODS: ConformanceMethod[] = [
  {
    id: "token-replay",
    name: "Token Replay",
    tagline:
      "Replay every trace against the model, counting missing and remaining tokens.",
    description:
      "Replays each trace on the chosen model (as a workflow net), tracking " +
      "produced, consumed, missing and remaining tokens to derive fitness and " +
      "precision overall and per variant.",
    artifact: "token-replay",
  },
  {
    id: "alignments",
    name: "Alignments",
    tagline:
      "Find each trace's cheapest path through the model, move by move.",
    description:
      "Computes the optimal alignment of every trace against the chosen " +
      "model: the least-cost sequence of synchronous, log-only and model-only " +
      "moves that walks the net to its end. Gives exact, optimal diagnostics " +
      "of where each trace deviates, plus cost-based fitness.",
    artifact: "alignments",
  },
  {
    id: "footprints",
    name: "Footprints",
    tagline:
      "Compare the directly-follows relations of the log and the model.",
    description:
      "Builds the footprint of the log and of the model — which activities " +
      "can directly follow which — and compares them. Reports how many of the " +
      "log's successions the model allows (fitness) and how many of the " +
      "model's the log shows (precision), and lists the deviating pairs. " +
      "Works even for an unsound net, where alignments cannot.",
    artifact: "footprints",
  },
  {
    id: "decisions",
    name: "Decisions",
    tagline:
      "Learn which attribute values steer a case down which branch.",
    description:
      "Finds the model's decision points — places where replay must choose " +
      "among several transitions — and mines, from the traces that replay " +
      "cleanly, rules over the case and event attributes in scope at each " +
      "choice: e.g. amount ≤ 1000 → Approve. Reports each rule's accuracy " +
      "against the always-guess-the-majority baseline.",
    artifact: "decisions",
  },
];

export const DEFAULT_METHOD = CONFORMANCE_METHODS[0];

/**
 * The activities whose alignments actually deviate — at least one inserted
 * (log) or skipped (model) move — keeping the backend's most-deviating-first
 * order. Activities that only ever align synchronously are noise in a
 * deviation table, so they are dropped.
 */
export function deviatingActivities(
  deviations: ActivityDeviation[] | undefined,
): ActivityDeviation[] {
  return (deviations ?? []).filter(
    (entry) => entry.log_moves + entry.model_moves > 0,
  );
}

/** One flattened decision rule: the conditions on the path to a leaf, the
 * branch the leaf predicts, and how many of its observations that covers. */
export interface DecisionRule {
  conditions: string[];
  branch: number;
  hit: number;
  total: number;
}

/** Renders one split's condition: "amount ≤ 1150" or "channel = web". */
export function splitCondition(node: DecisionTreeNode): string {
  if (node.threshold !== undefined) return `${node.key} ≤ ${node.threshold}`;
  return `${node.key} = ${node.value}`;
}

/**
 * Flattens a mined decision tree into one readable rule per leaf, in tree
 * order: then-subtree first, each else-path condition negated. A lone leaf
 * yields a single rule with no conditions.
 */
export function flattenDecisionRules(
  node: DecisionTreeNode,
  conditions: string[] = [],
): DecisionRule[] {
  if (node.branch !== undefined || !node.then || !node.else) {
    const total = node.counts.reduce((sum, count) => sum + count, 0);
    const branch = node.branch ?? 0;
    return [{ conditions, branch, hit: node.counts[branch] ?? 0, total }];
  }
  const condition = splitCondition(node);
  return [
    ...flattenDecisionRules(node.then, [...conditions, condition]),
    ...flattenDecisionRules(node.else, [...conditions, `not ${condition}`]),
  ];
}
