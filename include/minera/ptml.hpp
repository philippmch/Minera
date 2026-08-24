#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>

#include "minera/process_tree.hpp"

namespace minera {

// Serializes a process tree as PTML, the process-tree interchange format ProM
// and other process-mining tools read: one element per node — sequence, xor,
// and, or, xorLoop for the operators; manualTask for an activity leaf,
// automaticTask for a silent step — and one <parentsNode> edge per
// parent-child link, whose document order fixes the child order. A loop is
// written in the three-child shape those tools expect (body, redo, silent
// exit); several redo alternatives are bundled under one xor.
void write_ptml(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name = "process tree");

[[nodiscard]] std::string to_ptml(const ProcessTree& tree,
                                  std::string_view name = "process tree");

// Parses a PTML process tree, so a block-structured model exported elsewhere
// can be checked, played out, or re-exported like a mined one. The first
// <processTree> is read; the root is its `root` attribute, or the unique
// parentless node when the attribute is absent. A two-child xorLoop reads as
// body + redo; the three-child form's exit follows the loop in sequence (a
// silent exit simply disappears). An unnamed manualTask is a silent step,
// like an automaticTask. Operators with no children, dangling references, a
// node with two parents, cycles, and node kinds without an equivalent here
// (or, interleaving) all throw std::runtime_error.
[[nodiscard]] ProcessTree parse_ptml(std::string_view xml);

// Reads and parses a PTML file.
[[nodiscard]] ProcessTree import_ptml(const std::filesystem::path& path);

// True when the document's root element is <ptml> or a bare <processTree> —
// used to tell an uploaded process tree from a PNML net or BPMN model.
[[nodiscard]] bool looks_like_ptml(std::string_view xml);

}  // namespace minera
