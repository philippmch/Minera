// Registry of discovery miners shown in the "Mine a Process Model" and
// "Conformance" tabs. Grouped by miner family so new miners (and new variants
// of an existing family) can be added without touching the UI: append to
// MINER_GROUPS and, for a genuinely new output artifact, teach ProcessModel
// how to render it. An artifact the backend does not yet produce falls back to
// an honest "not available yet" placeholder.

export type ModelArtifact =
  | "process-tree"
  | "petri-net"
  | "dependency-graph"
  | "powl"
  | "log-skeleton"
  | "declare"
  | "footprints"
  | "transition-system";

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
        id: "classic", name: "Classic",
        description: "Run the native Classic Alpha algorithm over the active source and inspect its exact discovered workflow and exports.",
        artifact: "petri-net",
      },
      {
        id: "plus",
        name: "Alpha+",
        description:
          "Infers ordering, causality, parallelism and choice from the " +
          "directly-follows relations and assembles them into a Petri net. " +
          "The + variant also handles the classic algorithm's short-loop " +
          "blind spots: directly repeating activities become self loops, and " +
          "two alternating activities become a loop instead of being mistaken " +
          "for concurrency. Foundational, but sensitive to noise and " +
          "incomplete logs.",
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
        id: "plusplus", name: "Heuristics++",
        description: "Run interval-aware discovery with native timing, concurrency, performance and resource/lifecycle selectors, then inspect the dependency and converted workflow models.",
        artifact: "dependency-graph",
      },
      {
        id: "classic",
        name: "Classic",
        description:
          "Scores each ordering with a dependency measure that weighs how often " +
          "one activity follows another against how often it precedes it, then " +
          "keeps only edges above the dependency and frequency thresholds — " +
          "filtering the infrequent paths that mislead the Alpha Miner. An AND " +
          "measure decides per split and join which branches run in parallel " +
          "and which are exclusive, and conformance checks respect those " +
          "semantics.",
        artifact: "dependency-graph",
      },
    ],
  },
  {
    id: "powl",
    name: "POWL",
    tagline:
      "Partially ordered workflow languages: expressive choices, loops and concurrency with frequencies.",
    variants: [
      {
        id: "configured",
        name: "Configured discovery",
        description:
          "Discover a POWL model using the algorithm and filtering settings selected in Miner settings.",
        artifact: "powl",
      },
    ],
  },
  {
    id: "log_skeleton",
    name: "Log Skeleton",
    tagline:
      "Constraint-based discovery: equivalence, ordering, co-occurrence and activity frequencies.",
    variants: [
      {
        id: "classic",
        name: "Classic",
        description:
          "Mine a compact set of behavioral constraints from the event log and inspect their trace-level deviations.",
        artifact: "log-skeleton",
      },
    ],
  },
  {
    id: "declare",
    name: "Declare",
    tagline:
      "Declarative constraints: discover and inspect unary and binary templates with support and confidence.",
    variants: [
      {
        id: "classic",
        name: "Classic",
        description:
          "Mine all eighteen classic Declare template families, with optional activity/template filters and ratio thresholds.",
        artifact: "declare",
      },
    ],
  },
  {
    id: "footprints",
    name: "Footprints",
    tagline:
      "Aggregate process footprints across logs, graphs, trees, nets, and POWL models.",
    variants: [
      {
        id: "configured",
        name: "Configured discovery",
        description:
          "Discover and compare footprint variants using the settings selected in Miner settings.",
        artifact: "footprints",
      },
    ],
  },
];

MINER_GROUPS.push({
  id:"transition_system",name:"Transition System",
  tagline:"Inspect activity-context states, transitions and native traversal counts.",
  variants:[{id:"configured",name:"Configured discovery",description:"Render the sequence, set or multiset view discovered with the direction and window selected in Miner settings.",artifact:"transition-system"}],
});

MINER_GROUPS.push(
  {id:"ilp",name:"ILP Miner",tagline:"Discover a workflow net with configurable integer region search.",variants:[{id:"classic",name:"Classic",description:"Configure activity and boundary labels, causal relations, filtering and search limits, then inspect and export the discovered net.",artifact:"petri-net"}]},
  {id:"genetic",name:"Genetic Miner",tagline:"Evolve a causal matrix and inspect its converted workflow net.",variants:[{id:"classic",name:"Classic",description:"Configure population, generations, seed, evolutionary rates and input fields, then inspect the causal matrix and export its Petri net.",artifact:"petri-net"}]},
);

export const DEFAULT_GROUP = MINER_GROUPS.find((group) => group.id === "inductive")!;
export const DEFAULT_VARIANT = DEFAULT_GROUP.variants[0];
