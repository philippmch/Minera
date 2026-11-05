import type { ProcessTreeNode, ProcessTreePlayoutOptions } from "./types";

/** The server's EXTENSIVE process-tree playout limit on max_length
 * (/api/playout refuses a larger explicit or default value). */
export const SERVER_EXTENSIVE_MAX_LENGTH = 64;

/** The length of the tree's shortest trace, as EXTENSIVE playout reads it
 * for its default max_length: a silent leaf 0, an activity 1, a choice its
 * shortest child, a loop its body, any other block the sum of its children. */
export function processTreeShortestWordLength(root: ProcessTreeNode): number {
  // Post-order over an explicit stack, so deep trees cannot overflow.
  const lengths = new Map<ProcessTreeNode, number>();
  const pending: Array<{ node: ProcessTreeNode; expanded: boolean }> = [{ node: root, expanded: false }];
  while (pending.length) {
    const top = pending.pop()!;
    const children = top.node.children ?? [];
    if (!top.expanded && children.length) {
      pending.push({ node: top.node, expanded: true });
      for (const child of children) pending.push({ node: child, expanded: false });
      continue;
    }
    const of = (child: ProcessTreeNode) => lengths.get(child) ?? 0;
    let length: number;
    switch (top.node.operator) {
      case "tau": length = 0; break;
      case "activity": length = 1; break;
      case "exclusive":
      case "inclusive_or": length = children.length ? Math.min(...children.map(of)) : 0; break;
      case "loop": length = children.length ? of(children[0]) : 0; break;
      default: length = children.reduce((sum, child) => sum + of(child), 0);
    }
    lengths.set(top.node, length);
  }
  return lengths.get(root) ?? 0;
}

/** When an EXTENSIVE request on this tree cannot return traces from the
 * server: the shortest trace length, if it is above the server's max_length
 * limit (every trace is then longer than any allowed max_length, and an
 * omitted max_length defaults to that length, which the server refuses);
 * otherwise null. */
export function extensiveTreeTooLong(
  tree: ProcessTreeNode | undefined, options: ProcessTreePlayoutOptions,
): number | null {
  if (!tree || options.variant !== "EXTENSIVE") return null;
  const shortest = processTreeShortestWordLength(tree);
  return shortest > SERVER_EXTENSIVE_MAX_LENGTH ? shortest : null;
}
