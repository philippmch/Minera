#pragma once

// The errors pm4py's frame families raise on the frame of an XES upload that
// lacks a column they read (round FY, observed black-box): the Polars plan's
// ColumnNotFoundError over the frame itself, and the pandas KeyErrors.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "polars_column_hint.hpp"

namespace minera::detail {

// Python's repr of a str: single quotes unless the text holds a single quote
// and no double quote; backslash, the quote and control characters escaped.
inline std::string frame_python_str_repr(std::string_view text) {
  const bool double_quote = text.find('\'') != std::string_view::npos && text.find('"') == std::string_view::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const char ch : text) {
    const auto byte = static_cast<unsigned char>(ch);
    if (ch == '\\') out += "\\\\";
    else if (ch == quote) { out += '\\'; out += ch; }
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else if (ch == '\t') out += "\\t";
    else if (byte < 0x20 || byte == 0x7f) {
      static constexpr char digits[] = "0123456789abcdef";
      out += "\\x";
      out += digits[byte >> 4];
      out += digits[byte & 15];
    } else {
      out += ch;
    }
  }
  out += quote;
  return out;
}

// A column name as Polars quotes it in its messages and plans.
inline std::string polars_frame_quoted(const std::string& name) {
  std::string out = "\"";
  for (const char c : name) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

// Polars' ColumnNotFoundError for `missing` when a plan over the frame
// itself (columns `columns`) fails at its group_by: the valid columns, the
// "Did you mean" hint, the frame's DF node (four names, then "...") and the
// context stack, innermost call first.
inline std::string polars_df_missing_column_error(const std::string& missing, const std::vector<std::string>& columns,
                                                  const std::vector<std::string>& stack,
                                                  std::string_view failed_at = "group_by") {
  std::string message = "unable to find column " + polars_frame_quoted(missing) + "; valid columns: [";
  for (std::size_t i = 0; i < columns.size(); ++i) message += (i ? ", " : "") + polars_frame_quoted(columns[i]);
  message += "]";
  if (const auto hint = polars_column_hint(missing, columns)) message += "\n\nDid you mean " + polars_frame_quoted(*hint) + "?";
  std::string frame = "DF [";
  for (std::size_t i = 0; i < columns.size() && i < 4; ++i) frame += (i ? ", " : "") + polars_frame_quoted(columns[i]);
  if (columns.size() > 4) frame += ", ...";
  frame += "]; PROJECT */" + std::to_string(columns.size()) + " COLUMNS";
  message += "\n\nResolved plan until failure:\n\n\t---> FAILED HERE RESOLVING '" + std::string(failed_at) + "' <---\n" +
             frame + "\n\nThis error occurred with the following context stack:\n";
  for (std::size_t i = 0; i < stack.size(); ++i)
    message += "\t[" + std::to_string(i + 1) + "] '" + stack[i] + "'\n";
  return message;
}

// pandas' KeyError for a list selection (`frame.loc[:, keys]`) some of whose
// labels the frame lacks: the missing labels in selection order, or, when
// none is there, the whole selection as a str Index.
inline std::string pandas_list_selection_key_error(const std::vector<std::string>& keys,
                                                   const std::vector<std::string>& columns) {
  std::vector<std::string> missing;
  for (const auto& key : keys) {
    bool found = false;
    for (const auto& column : columns) found = found || column == key;
    if (!found) missing.push_back(key);
  }
  if (missing.empty()) return {};
  const auto listed = [](const std::vector<std::string>& names) {
    std::string out = "[";
    for (std::size_t i = 0; i < names.size(); ++i) out += (i ? ", " : "") + frame_python_str_repr(names[i]);
    return out + "]";
  };
  std::string text = missing.size() == keys.size()
      ? "None of [Index(" + listed(keys) + ", dtype='str')] are in the [columns]"
      : listed(missing) + " not in index";
  return "KeyError: " + frame_python_str_repr(text);
}

}  // namespace minera::detail
