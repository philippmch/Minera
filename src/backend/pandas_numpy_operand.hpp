#pragma once
// numpy's view of a tuple operand in a pandas Series comparison (rounds DS,
// DT; observed black-box). A Series of a numpy dtype hands a tuple to numpy,
// which makes it an array before comparing: lists and tuples nest as
// dimensions while every item of a level is one and all have one length; a
// level mixing them, or lengths, is ragged and raises. Leaves holding a str
// and no other object make a str array, for which the comparison has no loop.
// Otherwise the array broadcasts with the Series' (n,), every leaf compares
// with its rows in C order (a non-number raises its TypeError), and the
// result is assigned to the Series: its first dimension must be n and it
// must be one-dimensional.
#include "minera/event_log.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace minera::detail {

struct NumpyTupleArray {
  std::vector<std::size_t> shape;
  std::vector<const EventAttribute*> leaves;  // C order
};

inline bool numpy_operand_sequence(const EventAttribute& e) {
  return !e.is_null && (e.container == AttributeContainerKind::kList || e.container == AttributeContainerKind::kTuple);
}
inline bool numpy_operand_text(const EventAttribute& v) {
  return !v.is_null && v.container == AttributeContainerKind::kScalar &&
         (v.kind == AttributeKind::kString || v.kind == AttributeKind::kId) &&
         std::holds_alternative<std::monostate>(v.typed_value);
}
inline bool numpy_operand_number(const EventAttribute& v) {
  if (v.kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(v.typed_value)) return false;
  return v.kind == AttributeKind::kInt || v.kind == AttributeKind::kFloat || v.kind == AttributeKind::kBoolean ||
         std::holds_alternative<double>(v.typed_value) || std::holds_alternative<bool>(v.typed_value) ||
         std::holds_alternative<std::int64_t>(v.typed_value) || std::holds_alternative<std::uint64_t>(v.typed_value);
}
// "(2,)", "(2, 3)": numpy's shape as Python prints the tuple.
inline std::string numpy_python_shape(const std::vector<std::size_t>& shape) {
  std::string out = "(";
  for (std::size_t i = 0; i < shape.size(); ++i) out += (i ? ", " : "") + std::to_string(shape[i]);
  return out + (shape.size() == 1 ? ",)" : ")");
}
// "(2,)", "(2,3)": numpy's own spelling in broadcast errors.
inline std::string numpy_broadcast_shape(const std::vector<std::size_t>& shape) {
  std::string out = "(";
  for (std::size_t i = 0; i < shape.size(); ++i) out += (i ? "," : "") + std::to_string(shape[i]);
  return out + (shape.size() == 1 ? ",)" : ")");
}

// np.asarray(tuple): the shape and the leaves in C order, or numpy's
// inhomogeneous-shape ValueError.
template <class Error>
NumpyTupleArray numpy_tuple_array(const EventAttribute& tuple, Error&& error) {
  NumpyTupleArray out;
  out.shape.push_back(tuple.children.size());
  std::vector<const EventAttribute*> level;
  for (const auto& c : tuple.children) level.push_back(&c);
  while (!level.empty()) {
    const auto sequence = [](const EventAttribute* e) { return numpy_operand_sequence(*e); };
    if (!std::any_of(level.begin(), level.end(), sequence)) break;
    const auto width = level.front()->children.size();
    if (!std::all_of(level.begin(), level.end(), sequence) ||
        std::any_of(level.begin(), level.end(), [&](const EventAttribute* e) { return e->children.size() != width; }))
      error("ValueError",
            "setting an array element with a sequence. The requested array has an inhomogeneous shape after " +
                std::to_string(out.shape.size()) + " dimensions. The detected shape was " +
                numpy_python_shape(out.shape) + " + inhomogeneous part.");
    out.shape.push_back(width);
    std::vector<const EventAttribute*> next;
    for (const auto* e : level)
      for (const auto& c : e->children) next.push_back(&c);
    level = std::move(next);
  }
  out.leaves = std::move(level);
  return out;
}
// The number of dimensions np.asarray(tuple, dtype=object) gives: lists and
// tuples nest while every item of a level is one and all share a length.
inline std::size_t numpy_object_depth(const EventAttribute& tuple) {
  std::size_t depth = 1;
  std::vector<const EventAttribute*> level;
  for (const auto& c : tuple.children) level.push_back(&c);
  while (!level.empty()) {
    const auto width = level.front()->children.size();
    if (!std::all_of(level.begin(), level.end(), [&](const EventAttribute* e) {
          return numpy_operand_sequence(*e) && e->children.size() == width;
        }))
      break;
    ++depth;
    std::vector<const EventAttribute*> next;
    for (const auto* e : level)
      for (const auto& c : e->children) next.push_back(&c);
    level = std::move(next);
  }
  return depth;
}
// The leaves make a str array: every one a number or a str (no None, NA or
// other object) and at least one a str.
inline bool numpy_str_array(const NumpyTupleArray& a) {
  const auto plain = [](const EventAttribute* e) {
    return !e->is_null && e->container == AttributeContainerKind::kScalar && !is_native_na_attribute(*e) &&
           (numpy_operand_number(*e) || numpy_operand_text(*e));
  };
  return std::all_of(a.leaves.begin(), a.leaves.end(), plain) &&
         std::any_of(a.leaves.begin(), a.leaves.end(), [](const EventAttribute* e) { return numpy_operand_text(*e); });
}
inline std::string numpy_ufunc_name(const std::string& op) {
  return op == ">=" ? "greater_equal" : op == "<=" ? "less_equal" : op == "<" ? "less" : op == ">" ? "greater" : "equal";
}

// Series(op(values, np.asarray(tuple)), index): `compare(row, leaf)` gives
// one row against one leaf (raising its own TypeError); `dtype_class` is the
// Series' numpy dtype class (Float64DType, Int64DType).
template <class Compare, class Error>
std::vector<bool> numpy_tuple_mask(std::size_t n, const EventAttribute& bound, const std::string& op,
                                   const std::string& dtype_class, Compare&& compare, Error&& error) {
  const auto array = numpy_tuple_array(bound, error);
  if (numpy_str_array(array))
    error("UFuncTypeError", "ufunc '" + numpy_ufunc_name(op) +
                                "' did not contain a loop with signature matching types (<class 'numpy.dtypes." +
                                dtype_class + "'>, <class 'numpy.dtypes.StrDType'>) -> None");
  const auto& shape = array.shape;
  const auto last = shape.back();
  if (last != n && last != 1 && n != 1)
    error("ValueError", "operands could not be broadcast together with shapes (" + std::to_string(n) + ",) " +
                            numpy_broadcast_shape(shape) + " ");
  auto broadcast = shape;
  broadcast.back() = last == 1 ? n : last;
  const std::size_t outer = array.leaves.empty() || last == 0 ? 0 : array.leaves.size() / last;
  for (std::size_t o = 0; o < outer; ++o)
    for (std::size_t j = 0; j < broadcast.back(); ++j)
      (void)compare(n == 1 ? 0 : j, *array.leaves[o * last + (last == 1 ? 0 : j)]);
  if (broadcast.front() != n)
    error("ValueError", "Length of values (" + std::to_string(broadcast.front()) +
                            ") does not match length of index (" + std::to_string(n) + ")");
  if (broadcast.size() > 1)
    error("ValueError", "Data must be 1-dimensional, got ndarray of shape " + numpy_python_shape(broadcast) +
                            " instead");
  std::vector<bool> result(n);
  for (std::size_t i = 0; i < n; ++i) result[i] = compare(i, *array.leaves[last == 1 ? 0 : i]);
  return result;
}

}  // namespace minera::detail
