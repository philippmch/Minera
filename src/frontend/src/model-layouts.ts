// The diagrams of one analysis, laid out on demand. Laying out the process map,
// process tree and workflow nets of a large model blocks the page, so none is
// built when the analysis loads: each is built the first time a view (or an
// export drawn from that view) asks for it, then kept for that analysis. A new analysis (a
// new log, a re-mine, or the display fonts arriving) gets a new ModelLayouts,
// which drops every layout of the previous one.

import { computeLayout, type GraphLayout } from "./layout";
import { computePetriLayout, type PetriLayout } from "./petri-layout";
import { computeTreeLayout, type TreeLayout } from "./tree-layout";
import type { MineraExport, PetriNetModel } from "./types";

/** The workflow nets an analysis may carry, by the miner they belong to. */
export type ModelNet = "alpha" | "inductive" | "heuristic" | "powl";

export interface ModelLayouts {
  /** The directly-follows process map. */
  processMap(): GraphLayout;
  /** Whether the analysis carries a process tree (without laying it out). */
  hasTree(): boolean;
  /** The inductive miner's process tree, or null when the analysis has none. */
  tree(): TreeLayout | null;
  /** Whether the analysis carries that miner's workflow net (without laying it out). */
  hasNet(net: ModelNet): boolean;
  /** That miner's workflow net, or null when the analysis has none. */
  net(net: ModelNet): PetriLayout | null;
}

/** The layout functions; tests pass counting stand-ins. */
export interface LayoutBuilders {
  processMap: (data: MineraExport) => GraphLayout;
  tree: (root: NonNullable<MineraExport["process_tree"]>, data: MineraExport) => TreeLayout;
  net: (net: PetriNetModel, data: MineraExport) => PetriLayout;
}

const BUILDERS: LayoutBuilders = { processMap: computeLayout, tree: computeTreeLayout, net: computePetriLayout };

function netModel(data: MineraExport, net: ModelNet): PetriNetModel | null {
  switch (net) {
    case "alpha": return data.petri_net ?? null;
    case "inductive": return data.inductive_petri_net ?? null;
    case "heuristic": return data.heuristic_petri_net ?? null;
    case "powl": return data.powl_petri_net ?? null;
  }
}

export function modelLayouts(data: MineraExport, builders: LayoutBuilders = BUILDERS): ModelLayouts {
  let processMap: GraphLayout | undefined;
  let tree: TreeLayout | null | undefined;
  const nets = new Map<ModelNet, PetriLayout | null>();
  return {
    processMap: () => (processMap ??= builders.processMap(data)),
    hasTree: () => Boolean(data.process_tree),
    tree: () => {
      if (tree === undefined) tree = data.process_tree ? builders.tree(data.process_tree, data) : null;
      return tree;
    },
    hasNet: (net) => netModel(data, net) !== null,
    net: (net) => {
      if (!nets.has(net)) {
        const model = netModel(data, net);
        nets.set(net, model ? builders.net(model, data) : null);
      }
      return nets.get(net) ?? null;
    },
  };
}
