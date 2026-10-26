// The registry of standalone tools and of the views inside an open log. The
// landing page, the sidebar and the command palette all render from these
// lists, so adding a tool means adding one entry here and one render case in
// App.tsx.

import type { Tab } from "./app-logic";

export type ToolGroupId =
  | "data"
  | "discover"
  | "conformance"
  | "models"
  | "simulate"
  | "time"
  | "organization"
  | "privacy"
  | "assist";

export interface ToolGroup {
  id: ToolGroupId;
  title: string;
  blurb: string;
}

export const TOOL_GROUPS: readonly ToolGroup[] = [
  { id: "data", title: "Data and sources", blurb: "Import, acquire, query and reshape event data." },
  { id: "discover", title: "Discovery", blurb: "Discover process models from supplied inputs." },
  { id: "conformance", title: "Conformance", blurb: "Replay, align and score logs against models." },
  { id: "models", title: "Models", blurb: "Open, convert and analyse process models." },
  { id: "simulate", title: "Simulation", blurb: "Play out models and generate synthetic logs." },
  { id: "time", title: "Performance and time", blurb: "Timing, temporal profiles and concept drift." },
  { id: "organization", title: "Organization and decisions", blurb: "Resources, decisions, root causes and clusters." },
  { id: "privacy", title: "Privacy", blurb: "Anonymize logs before sharing them." },
  { id: "assist", title: "Assistant", blurb: "Ask a language model about your process." },
];

export type ToolId =
  | "active-source"
  | "ocel-io"
  | "acquisition"
  | "related-logs"
  | "streaming"
  | "process-cube"
  | "log-query"
  | "features"
  | "labels"
  | "embedding-filter"
  | "saved-structures"
  | "workflow-inputs"
  | "inductive-inputs"
  | "ilp-genetic"
  | "genetic-matrix"
  | "tree-generation"
  | "token-replay"
  | "petri-alignment"
  | "graph-alignment"
  | "tree-alignment"
  | "anti-multi"
  | "edit-distance"
  | "declare"
  | "log-skeleton"
  | "footprints"
  | "quality"
  | "emd"
  | "model-exchange"
  | "model-conversion"
  | "petri-analysis"
  | "monte-carlo"
  | "playout"
  | "oc-playout"
  | "timing"
  | "time-analysis"
  | "temporal"
  | "bose"
  | "organizational"
  | "decisions"
  | "root-cause"
  | "clustering"
  | "anonymization"
  | "prompt-chat";

export interface ToolSpec {
  id: ToolId;
  group: ToolGroupId;
  title: string;
  summary: string;
  /** Extra words the search matches besides the title and summary. */
  keywords?: string;
}

export const TOOLS: readonly ToolSpec[] = [
  { id: "active-source", group: "data", title: "Source inspector", summary: "Open a CSV, XES, table or JSON source, choose its columns and mining options, and inspect what the core reads.", keywords: "native mining source import columns" },
  { id: "ocel-io", group: "data", title: "OCEL import and export", summary: "Import OCEL JSON, XML, CSV or SQLite files and convert between the formats.", keywords: "object-centric ocel2" },
  { id: "acquisition", group: "data", title: "Acquire source data", summary: "Extract events from browser history, Git, Outlook, SAP, Camunda and Windows event logs.", keywords: "connector extractor" },
  { id: "related-logs", group: "data", title: "Relate logs", summary: "Inspect event links, timestamp crossings and case-relation merges across logs.", keywords: "interleavings merge dataframe" },
  { id: "streaming", group: "data", title: "Event streams", summary: "Replay CSV or XES files as event streams and follow the streaming results batch by batch.", keywords: "live online reader declare monitor" },
  { id: "process-cube", group: "data", title: "Process cubes", summary: "Compare cases across two numeric, categorical or composite dimensions.", keywords: "dimensions slice dice" },
  { id: "log-query", group: "data", title: "SQL and log queries", summary: "Run native SQL and queries over logs and tables, then open the result as a new log.", keywords: "sql filter table dataframe" },
  { id: "features", group: "data", title: "Log features", summary: "Extract event, case or temporal features from a log.", keywords: "machine learning vectors" },
  { id: "labels", group: "data", title: "Label splitting", summary: "Split activity labels by the context they occur in.", keywords: "relabel refine" },
  { id: "embedding-filter", group: "data", title: "Embeddings and filters", summary: "Filter logs with the native filters, or embed logs and literal text.", keywords: "vector embedding" },
  { id: "saved-structures", group: "data", title: "Saved structures", summary: "Convert a saved trie, or query a saved list of intervals.", keywords: "trie interval tree" },

  { id: "workflow-inputs", group: "discover", title: "Workflow discovery", summary: "Upload a log, an event table or a DFG and discover a workflow model from it.", keywords: "alpha heuristics miner" },
  { id: "inductive-inputs", group: "discover", title: "Inductive discovery", summary: "Discover an Inductive process tree from a log, an event table, weighted variants or a DFG.", keywords: "inductive miner imf imd tree" },
  { id: "ilp-genetic", group: "discover", title: "ILP and genetic mining", summary: "Run the ILP and genetic miners on a supplied log and inspect every step.", keywords: "integer linear programming evolutionary" },
  { id: "genetic-matrix", group: "discover", title: "Causal matrix to Petri net", summary: "Convert a genetic causal matrix into a workflow net.", keywords: "genetic matrix" },
  { id: "tree-generation", group: "discover", title: "Generate process trees", summary: "Generate random process trees and play them out into logs.", keywords: "synthetic random tree" },

  { id: "token-replay", group: "conformance", title: "Token replay", summary: "Replay a log on a PNML model and inspect missing, remaining and consumed tokens.", keywords: "fitness pnml" },
  { id: "petri-alignment", group: "conformance", title: "Petri net alignment", summary: "Align a log against a PNML model with configurable costs.", keywords: "optimal alignment pnml" },
  { id: "graph-alignment", group: "conformance", title: "DFG and graph alignment", summary: "Align traces against a directly-follows graph or a PNML model.", keywords: "dfg alignment" },
  { id: "tree-alignment", group: "conformance", title: "Process tree alignment", summary: "Align traces against a process tree with the search, DP or MILP variants.", keywords: "tree alignment milp" },
  { id: "anti-multi", group: "conformance", title: "Anti- and multi-alignment", summary: "Compare a set of traces with a PNML model through anti- and multi-alignments.", keywords: "anti-alignment multi-alignment" },
  { id: "edit-distance", group: "conformance", title: "Edit-distance alignment", summary: "Align or project event sequences against each other by edit distance.", keywords: "levenshtein" },
  { id: "declare", group: "conformance", title: "Declare models", summary: "Check a log against a Declare model or play the model out.", keywords: "constraints ltl" },
  { id: "log-skeleton", group: "conformance", title: "Log skeleton checks", summary: "Check a log against a log skeleton and diagnose the results.", keywords: "constraints" },
  { id: "footprints", group: "conformance", title: "Footprints comparison", summary: "Compare log and model footprints and inspect the conformance metrics.", keywords: "footprint matrix" },
  { id: "quality", group: "conformance", title: "Model quality", summary: "Score fitness, precision, generalization and simplicity of a model against a log.", keywords: "evaluation metrics" },
  { id: "emd", group: "conformance", title: "Stochastic language distance", summary: "Compare two weighted stochastic languages with the earth mover's distance.", keywords: "emd stochastic" },

  { id: "model-exchange", group: "models", title: "Model files", summary: "Open PNML, BPMN, PTML and DFG files, view them and convert between formats.", keywords: "import export bpmn pnml" },
  { id: "model-conversion", group: "models", title: "Convert and reduce models", summary: "Convert a process model to another notation, or reduce it.", keywords: "conversion reduction" },
  { id: "petri-analysis", group: "models", title: "Petri net analysis", summary: "Check soundness, reachability and structural properties of a Petri net.", keywords: "soundness workflow net reachability" },

  { id: "monte-carlo", group: "simulate", title: "Monte Carlo simulation", summary: "Simulate a PNML model with fitted or supplied timing distributions.", keywords: "simulation stochastic" },
  { id: "playout", group: "simulate", title: "Model playout", summary: "Sample or enumerate the behavior of a Petri net, DFG or process tree.", keywords: "synthetic log generate" },
  { id: "oc-playout", group: "simulate", title: "Object-centric playout", summary: "Play out an object-centric Petri net or causal net into an object-centric log.", keywords: "ocpn ocel" },

  { id: "timing", group: "time", title: "Performance spectra and batches", summary: "Compute performance spectra and detect batches in a supplied log.", keywords: "spectrum batch" },
  { id: "time-analysis", group: "time", title: "Case and service times", summary: "Measure passed, service, cycle and case times.", keywords: "duration lead time" },
  { id: "temporal", group: "time", title: "Temporal profiles", summary: "Build temporal profiles and check a log against them.", keywords: "temporal profile deviation" },
  { id: "bose", group: "time", title: "Concept drift", summary: "Detect concept drift with the Bose method and inspect its statistics.", keywords: "bose drift change point" },

  { id: "organizational", group: "organization", title: "Social networks and roles", summary: "Discover handover networks, roles and resource profiles.", keywords: "sna resources handover working together" },
  { id: "decisions", group: "organization", title: "Decision mining", summary: "Learn the decision rules behind the choices in a PNML model.", keywords: "decision tree guards" },
  { id: "root-cause", group: "organization", title: "Root cause analysis", summary: "Compare two cohorts of cases and find the attributes that separate them.", keywords: "cohort" },
  { id: "clustering", group: "organization", title: "Trace clustering", summary: "Cluster the cases of a log and inspect each cluster.", keywords: "cluster" },

  { id: "anonymization", group: "privacy", title: "Anonymization", summary: "Anonymize a log or release differentially private trace-variant queries.", keywords: "privacy sacofa pripel differential" },

  { id: "prompt-chat", group: "assist", title: "Ask a model", summary: "Send a prompt to a language model without opening a dataset.", keywords: "llm chat openai anthropic" },
];

const toolIndex = new Map(TOOLS.map((tool) => [tool.id, tool]));

export function toolById(id: ToolId): ToolSpec {
  const tool = toolIndex.get(id);
  if (!tool) throw new Error(`unknown tool ${id}`);
  return tool;
}

export function toolGroup(id: ToolGroupId): ToolGroup {
  return TOOL_GROUPS.find((group) => group.id === id)!;
}

function normalize(text: string): string {
  return text.toLowerCase().normalize("NFKD").replace(/[̀-ͯ]/g, "");
}

/** Whether every word of the query appears in the item's searchable text. */
export function matchesQuery(query: string, ...fields: (string | undefined)[]): boolean {
  const words = normalize(query).split(/\s+/).filter(Boolean);
  if (!words.length) return true;
  const haystack = normalize(fields.filter(Boolean).join(" "));
  return words.every((word) => haystack.includes(word));
}

export function searchTools(query: string, tools: readonly ToolSpec[] = TOOLS): ToolSpec[] {
  return tools.filter((tool) => matchesQuery(query, tool.title, tool.summary, tool.keywords, toolGroup(tool.group).title));
}

export function groupTools(tools: readonly ToolSpec[] = TOOLS): { group: ToolGroup; tools: ToolSpec[] }[] {
  return TOOL_GROUPS.map((group) => ({ group, tools: tools.filter((tool) => tool.group === group.id) })).filter((entry) => entry.tools.length > 0);
}

// Views of an open event log, grouped the way the sidebar lists them.

export type ViewGroupId = "overview" | "discover" | "check" | "analyze" | "prepare" | "assist";

export interface ViewSpec {
  id: Tab;
  group: ViewGroupId;
  title: string;
  summary: string;
}

export const VIEW_GROUPS: readonly { id: ViewGroupId; title: string }[] = [
  { id: "overview", title: "Overview" },
  { id: "discover", title: "Discover" },
  { id: "check", title: "Check" },
  { id: "analyze", title: "Analyze" },
  { id: "prepare", title: "Prepare" },
  { id: "assist", title: "Assist" },
];

export const VIEWS: readonly ViewSpec[] = [
  { id: "log", group: "overview", title: "Event log", summary: "Every case and event in the log, with exports." },
  { id: "dotted", group: "overview", title: "Timeline", summary: "Dotted chart, performance spectrum and batches." },
  { id: "dfg", group: "discover", title: "Process map", summary: "The directly-follows graph with variants and activity statistics." },
  { id: "model", group: "discover", title: "Process models", summary: "Mine Petri nets, trees, POWL, Declare and other models." },
  { id: "timing", group: "discover", title: "Timing", summary: "Discover performance spectra, batches and temporal relations." },
  { id: "behavior", group: "discover", title: "Behavior", summary: "Footprints, transition systems, Declare and log skeleton constraints." },
  { id: "conformance", group: "check", title: "Conformance", summary: "Replay and align the log against the mined or an imported model." },
  { id: "resources", group: "analyze", title: "Resources", summary: "Social networks, roles and organizational mining." },
  { id: "clustering", group: "analyze", title: "Clustering", summary: "Cluster cases and test hypotheses on the clusters." },
  { id: "embeddings", group: "analyze", title: "Search and embeddings", summary: "Find similar cases and embed the log." },
  { id: "query", group: "prepare", title: "Query and filter", summary: "SQL queries and natural-language filters that open as a new log." },
  { id: "transformation", group: "prepare", title: "Transform", summary: "Feature, label and log transformations." },
  { id: "anonymization", group: "prepare", title: "Anonymize", summary: "Anonymize the log before sharing it." },
  { id: "llm", group: "assist", title: "Ask a model", summary: "Ask a language model about this log and its views." },
];

export function viewById(id: Tab): ViewSpec {
  return VIEWS.find((view) => view.id === id)!;
}
