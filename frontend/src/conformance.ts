// Registry of conformance-checking methods shown in the "Conformance" tab.
// Mirrors miners.ts: new methods can be added without touching the chooser UI
// by appending to CONFORMANCE_METHODS, and — only for a genuinely new result
// shape — teaching Conformance how to render that artifact.

export type ConformanceArtifact = "token-replay";

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
      "Replays each trace on the inductive-miner process tree (as a workflow " +
      "net), tracking produced, consumed, missing and remaining tokens to " +
      "derive fitness and precision overall and per variant.",
    artifact: "token-replay",
  },
];

export const DEFAULT_METHOD = CONFORMANCE_METHODS[0];
