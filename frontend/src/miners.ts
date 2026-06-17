// Registry of discovery algorithms shown in the "Mine a process model" tab.
// Grouped by algorithm family so new miners (and new variants of an existing
// family) can be added without touching the UI: append to MINER_GROUPS and,
// for a genuinely new output artifact, teach ProcessModel how to render it.

export type ModelArtifact = "process-tree";

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
];

export const DEFAULT_GROUP = MINER_GROUPS[0];
export const DEFAULT_VARIANT = MINER_GROUPS[0].variants[0];
