// Registry of discovery miners shown in the "Mine a Process Model" and
// "Conformance" tabs. Grouped by miner family so new miners (and new variants
// of an existing family) can be added without touching the UI: append to
// MINER_GROUPS and, for a genuinely new output artifact, teach ProcessModel
// how to render it. An artifact the backend does not yet produce falls back to
// an honest "not available yet" placeholder.

export type ModelArtifact = "process-tree" | "petri-net" | "dependency-graph";

export interface MinerVariant {
  id: string;
  name: string;
  description: string;
  /** Which model representation this variant produces. */
  artifact: ModelArtifact;
}

export interface MinerGroup {
  id: string;
  name: string;
  /** One-line summary shown on the algorithm-selection card. */
  tagline: string;
  variants: MinerVariant[];
}

export const MINER_GROUPS: MinerGroup[] = [
  {
    id: "inductive",
    name: "Inductive Miner",
    tagline:
      "Block-structured process trees, guaranteed sound and perfectly fitting.",
    variants: [
      {
        id: "basic",
        name: "Basic",
        description:
          "Recursively cuts the directly-follows graph into exclusive-choice, " +
          "sequence, parallel and loop operators, yielding a block-structured " +
          "process tree that is always sound and perfectly fits the log.",
        artifact: "process-tree",
      },
    ],
  },
  {
    id: "alpha",
    name: "Alpha Miner",
    tagline:
      "The classic algorithm: derives a Petri net from directly-follows relations.",
    variants: [
      {
        id: "classic",
        name: "Classic",
        description:
          "Infers ordering, causality, parallelism and choice from the " +
          "directly-follows relations and assembles them into a Petri net. " +
          "Foundational, but sensitive to noise and incomplete logs.",
        artifact: "petri-net",
      },
    ],
  },
  {
    id: "heuristic",
    name: "Heuristic Miner",
    tagline:
      "Noise-tolerant: keeps only the dependencies frequent and reliable enough to trust.",
    variants: [
      {
        id: "classic",
        name: "Classic",
        description:
          "Scores each ordering with a dependency measure that weighs how often " +
          "one activity follows another against how often it precedes it, then " +
          "keeps only edges above the dependency and frequency thresholds — " +
          "filtering the infrequent paths that mislead the Alpha Miner.",
        artifact: "dependency-graph",
      },
    ],
  },
];

export const DEFAULT_GROUP = MINER_GROUPS[0];
export const DEFAULT_VARIANT = MINER_GROUPS[0].variants[0];
