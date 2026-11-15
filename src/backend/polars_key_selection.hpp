#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera::detail::polars_keys {

// Round CM: column keys given as a dict (or a list of names) to the Polars
// table filters, observed black-box against Polars 1.44.1:
// - pl.col(dict) selects the dict's keys by name, exactly like pl.col(list):
//   one existing name acts as that column, a name no column carries fails
//   `"k" not found` (no valid-columns list, no hint) at the node where a
//   missing str column fails, and {} selects nothing (the empty-list case);
// - group_by, sort, unique and over raise a TypeError on a dict while the
//   expressions are built, before anything is collected.
// The model keeps a by-name key whose names miss a column as the first
// missing name behind a \x01 marker, so the filters' missing-column paths
// fail at their usual node and only the message changes.
constexpr char kByNameMarker = '\x01';

inline bool by_name(const std::string& key) { return !key.empty() && key.front() == kByNameMarker; }
inline std::string by_name_column(const std::string& key) { return by_name(key) ? key.substr(1) : key; }
inline std::string by_name_message(const std::string& key) { return '"' + by_name_column(key) + "\" not found"; }

inline bool dict_key(const EventAttribute& key) { return key.container == AttributeContainerKind::kContainer; }

inline const char* dict_argument_message() {
  return "Cannot pass a dictionary as a single positional argument.\n"
         "If you merely want the *keys*, use:\n"
         "  \xe2\x80\xa2 df.method(*your_dict.keys())\n"
         "If you need the key value pairs, use one of:\n"
         "  \xe2\x80\xa2 unpack as keywords:    df.method(**your_dict)\n"
         "  \xe2\x80\xa2 build expressions:     df.method(expr.alias(k) for k, expr in your_dict.items())";
}

// A plain str scalar (not null, not a container, no typed payload).
inline bool plain_text(const EventAttribute& v) {
  return v.container == AttributeContainerKind::kScalar && !v.is_null && v.kind == AttributeKind::kString;
}

// The key as pl.col reads it: a dict becomes the list of its names; a
// non-empty list of str names becomes the one existing name, the first
// missing name behind the by-name marker, or stays a list when it names
// several existing columns (outside the modelled domain). Anything else
// is returned unchanged.
inline EventAttribute col_key(const EventAttribute& key, const std::vector<std::string>& columns) {
  if (key.container == AttributeContainerKind::kScalar) return key;
  std::vector<std::string> names;
  if (dict_key(key)) {
    for (const auto& child : key.children)
      if (std::find(names.begin(), names.end(), child.key) == names.end()) names.push_back(child.key);
  } else if (key.container == AttributeContainerKind::kList || key.container == AttributeContainerKind::kTuple) {
    for (const auto& item : key.children) {
      if (!plain_text(item)) return key;
      if (std::find(names.begin(), names.end(), item.value) == names.end()) names.push_back(item.value);
    }
  } else {
    return key;
  }
  if (names.empty()) {
    EventAttribute empty = key;
    empty.container = AttributeContainerKind::kList;
    empty.children.clear();
    return empty;
  }
  for (const auto& name : names)
    if (std::find(columns.begin(), columns.end(), name) == columns.end()) return EventAttribute{key.key, std::string(1, kByNameMarker) + name};
  if (names.size() == 1) return EventAttribute{key.key, names.front()};
  if (!dict_key(key)) return key;
  EventAttribute list{key.key, ""};
  list.container = AttributeContainerKind::kList;
  for (const auto& name : names) list.children.push_back(EventAttribute{std::to_string(list.children.size()), name});
  return list;
}

}  // namespace minera::detail::polars_keys
