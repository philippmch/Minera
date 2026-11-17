#pragma once

#include <string>

namespace minera::detail {

// Round DL: pm4py's Polars filters called on an eager DataFrame run each
// step as it is called, so a failing step reports no resolved plan; only a
// group_by runs through a lazy plan of its own, over its eager input frame
// (context stack `[1] 'group_by'`). The lazy model's diagnostic becomes the
// eager one: a failure whose context stack starts at a group_by over the
// frame itself keeps that frame as its plan, any other loses the plan.
// A group_by over a derived frame is left as the lazy model renders it
// (outside what the eager probes have shown; probe dl/e1).
inline std::string polars_eager_diagnostic(const std::string& message) {
  static const std::string plan = "\n\nResolved plan until failure:";
  static const std::string stack = "\n\nThis error occurred with the following context stack:\n";
  const auto at = message.find(plan);
  if (at == std::string::npos) return message;
  const auto head = message.substr(0, at);
  const auto context = message.find(stack, at);
  if (context == std::string::npos) return head;
  const auto nodes = message.substr(context + stack.size());
  if (nodes.rfind("\t[1] 'group_by'\n", 0) != 0) return head;
  // The plan text below the FAILED HERE line, up to the context stack.
  const auto failed = message.find("<---\n", at);
  if (failed == std::string::npos || failed > context) return message;
  const auto input = message.substr(failed + 5, context - failed - 5);
  if (input.rfind("DF [", 0) != 0 || input.find('\n') != std::string::npos) return message;
  return head + plan + "\n\n\t---> FAILED HERE RESOLVING 'group_by' <---\n" + input + stack + "\t[1] 'group_by'\n";
}

}  // namespace minera::detail
