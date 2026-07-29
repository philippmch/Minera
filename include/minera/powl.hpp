#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace minera {

enum class PowlNodeKind {
  kTransition,
  kSilent,
  kXor,
  kLoop,
  kStrictPartialOrder,
};

struct PowlFrequency {
  int minimum = 1;                         // Minimum frequency: 0 or 1
  std::optional<int> maximum = 1;          // one, or null for unbounded
  friend bool operator==(const PowlFrequency&, const PowlFrequency&) = default;
};

class PowlNode {
 public:
  PowlNode() = delete;

  static PowlNode transition(std::string label,
                             PowlFrequency frequency = {});
  static PowlNode silent();
  static PowlNode xor_node(std::vector<PowlNode> children);
  static PowlNode loop(PowlNode body, PowlNode redo);

  // Constructs a strict partial order. `order` may contain cover edges or a
  // complete relation; it is canonicalized to its transitive closure.
  static PowlNode strict_partial_order(
      std::vector<PowlNode> children,
      const std::vector<std::pair<std::size_t, std::size_t>>& order);
  static PowlNode sequence(std::vector<PowlNode> children);
  static PowlNode concurrency(std::vector<PowlNode> children);

  [[nodiscard]] PowlNodeKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::string& label() const noexcept { return label_; }
  [[nodiscard]] const PowlFrequency& frequency() const noexcept {
    return frequency_;
  }
  [[nodiscard]] bool skippable() const noexcept {
    return kind_ == PowlNodeKind::kTransition && frequency_.minimum == 0;
  }
  [[nodiscard]] bool self_loop() const noexcept {
    return kind_ == PowlNodeKind::kTransition &&
           (!frequency_.maximum || *frequency_.maximum > 1);
  }
  [[nodiscard]] const std::vector<PowlNode>& children() const noexcept {
    return children_;
  }
  [[nodiscard]] const std::vector<std::pair<std::size_t, std::size_t>>&
  order() const noexcept {
    return order_;
  }

  // Relation queries are defined only for kStrictPartialOrder and reject all
  // other node kinds (and out-of-range indices).
  [[nodiscard]] bool has_edge(std::size_t source, std::size_t target) const;
  [[nodiscard]] std::vector<std::size_t> start_nodes() const;
  [[nodiscard]] std::vector<std::size_t> end_nodes() const;
  [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>>
  transitive_reduction() const;

  [[nodiscard]] std::string serialize() const;
  static PowlNode parse(const std::string& text);

  friend bool operator==(const PowlNode& lhs, const PowlNode& rhs) noexcept;
  friend bool operator!=(const PowlNode& lhs, const PowlNode& rhs) noexcept {
    return !(lhs == rhs);
  }

 private:
  PowlNode(PowlNodeKind kind, std::string label,
           PowlFrequency frequency, std::vector<PowlNode> children,
           std::vector<std::pair<std::size_t, std::size_t>> order);

  PowlNodeKind kind_;
  std::string label_;
  PowlFrequency frequency_;
  std::vector<PowlNode> children_;
  std::vector<std::pair<std::size_t, std::size_t>> order_;
};

}  // namespace minera
