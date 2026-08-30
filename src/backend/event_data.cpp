#include "minera/event_data.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace minera {
namespace {

void validate_columns(const std::vector<std::string>& columns) {
  std::unordered_set<std::string> seen;
  seen.reserve(columns.size());
  for (const auto& column : columns) {
    if (column.empty())
      throw std::invalid_argument("event table: column names must not be empty");
    if (!seen.insert(column).second)
      throw std::invalid_argument("event table: duplicate column name: " +
                                  column);
  }
}

}  // namespace

const EventAttribute* EventRecord::field(std::string_view key) const {
  const auto found = std::find_if(
      fields.begin(), fields.end(), [&](const auto& field) {
        return field.key == key;
      });
  return found == fields.end() ? nullptr : &*found;
}

void EventTable::validate() const {
  validate_columns(columns);
  for (const auto& row : rows) {
    if (row.size() != columns.size())
      throw std::invalid_argument(
          "event table: row width does not match column count");
  }
}

std::size_t EventTable::column_index(std::string_view name) const {
  validate_columns(columns);
  const auto found = std::find(columns.begin(), columns.end(), name);
  if (found == columns.end())
    throw std::out_of_range("event table: column not found: " +
                            std::string(name));
  return static_cast<std::size_t>(found - columns.begin());
}

LazyEventTable::LazyEventTable(std::vector<std::string> columns,
                               Scanner scanner,
                               std::vector<EventAttribute> attributes,
                               EventDataMetadata metadata,
                               std::optional<std::size_t> row_count)
    : columns_(std::move(columns)),
      scanner_(std::move(scanner)),
      attributes_(std::move(attributes)),
      metadata_(std::move(metadata)),
      row_count_(row_count) {
  validate_columns(columns_);
  if (!scanner_)
    throw std::invalid_argument("lazy event table: scanner is empty");
}

void LazyEventTable::scan(const RowConsumer& consumer,
                          std::optional<std::size_t> max_rows) const {
  if (!scanner_)
    throw std::invalid_argument("lazy event table: scanner is empty");
  if (max_rows && row_count_ && *row_count_ > *max_rows)
    throw std::length_error("lazy event table: row materialization cap exceeded");

  std::size_t count = 0;
  scanner_([&](const Row& row) {
    if (row.size() != columns_.size())
      throw std::invalid_argument(
          "lazy event table: scanner produced a row with invalid width");
    if (max_rows && count >= *max_rows)
      throw std::length_error(
          "lazy event table: row materialization cap exceeded");
    if (consumer) consumer(row);
    ++count;
  });
}

EventTable LazyEventTable::materialize(
    std::optional<std::size_t> max_rows) const {
  EventTable table;
  table.columns = columns_;
  table.attributes = attributes_;
  table.metadata = metadata_;
  if (row_count_) {
    if (max_rows && *row_count_ > *max_rows)
      throw std::length_error(
          "lazy event table: row materialization cap exceeded");
    table.rows.reserve(*row_count_);
  }
  scan([&](const Row& row) { table.rows.push_back(row); }, max_rows);
  table.validate();
  return table;
}

LazyEventTable LazyEventTable::from_table(const EventTable& source) {
  source.validate();
  auto table = std::make_shared<EventTable>(source);
  return LazyEventTable(
      table->columns,
      [table](const RowConsumer& consumer) {
        for (const auto& row : table->rows) consumer(row);
      },
      table->attributes, table->metadata, table->rows.size());
}

}  // namespace minera
