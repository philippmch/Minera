// Registry of conformance-checking methods shown in the "Conformance" tab.
// Mirrors miners.ts: new methods can be added without touching the chooser UI
// by appending to CONFORMANCE_METHODS, and — only for a genuinely new result
// shape — teaching Conformance how to render that artifact.

export type ConformanceArtifact = "token-replay" | "alignments";

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
];

export const DEFAULT_METHOD = CONFORMANCE_METHODS[0];
