#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// A row in a flat event stream.  The order of fields is significant: it is
// the order supplied by the source adapter and is retained by table output.
struct EventRecord {
  std::vector<EventAttribute> fields;

  [[nodiscard]] const EventAttribute* field(std::string_view key) const;
};

// A value-semantic flat event stream.  Metadata is kept alongside the rows so
// conversions do not silently discard log-level declarations and globals.
struct EventStream {
  std::vector<EventAttribute> attributes;
  std::vector<EventRecord> events;
  EventDataMetadata metadata;

  [[nodiscard]] std::size_t size() const noexcept { return events.size(); }
  [[nodiscard]] bool empty() const noexcept { return events.empty(); }
};

// A rectangular event table.  An empty optional is a missing cell.  A cell
// containing an EventAttribute with is_null=true is a present null.  An
// ordinary value with value=="" is a present empty string.
struct EventTable {
  using Row = std::vector<std::optional<EventAttribute>>;

  std::vector<std::string> columns;
  std::vector<Row> rows;
  std::vector<EventAttribute> attributes;
  EventDataMetadata metadata;

  [[nodiscard]] std::size_t size() const noexcept { return rows.size(); }
  [[nodiscard]] bool empty() const noexcept { return rows.empty(); }

  // Validate that the table has unique, non-empty columns and rectangular
  // rows.  Conversion functions call this before reading a table.
  void validate() const;
  [[nodiscard]] std::size_t column_index(std::string_view name) const;
};

// A restartable, value-semantic table producer.  The scanner may be backed by
// a file, a database, or an in-memory table.  Each scan starts from the first
// row, and materialize() enforces an optional hard row cap.
class LazyEventTable {
 public:
  using Row = EventTable::Row;
  using RowConsumer = std::function<void(const Row&)>;
  using Scanner = std::function<void(const RowConsumer&)>;

  LazyEventTable() = default;
  LazyEventTable(std::vector<std::string> columns, Scanner scanner,
                 std::vector<EventAttribute> attributes = {},
                 EventDataMetadata metadata = {},
                 std::optional<std::size_t> row_count = std::nullopt);

  [[nodiscard]] const std::vector<std::string>& columns() const noexcept {
    return columns_;
  }
  [[nodiscard]] const std::vector<EventAttribute>& attributes() const noexcept {
    return attributes_;
  }
  [[nodiscard]] const EventDataMetadata& metadata() const noexcept {
    return metadata_;
  }
  [[nodiscard]] std::optional<std::size_t> row_count() const noexcept {
    return row_count_;
  }
  [[nodiscard]] bool valid() const noexcept {
    return static_cast<bool>(scanner_);
  }

  // Invoke the scanner from the beginning.  If max_rows is set, exceeding
  // the cap raises std::length_error rather than silently truncating data.
  void scan(const RowConsumer& consumer,
            std::optional<std::size_t> max_rows = std::nullopt) const;

  [[nodiscard]] EventTable materialize(
      std::optional<std::size_t> max_rows = std::nullopt) const;

  [[nodiscard]] static LazyEventTable from_table(const EventTable& table);

 private:
  std::vector<std::string> columns_;
  Scanner scanner_;
  std::vector<EventAttribute> attributes_;
  EventDataMetadata metadata_;
  std::optional<std::size_t> row_count_;
};

enum class EventDataKind { kEventLog, kEventStream, kEventTable, kLazyEventTable };

// A non-owning tagged input view used by generic conversion entry points.
// The referenced object must outlive the view and the conversion call.
class EventDataView {
 public:
  explicit EventDataView(const EventLog& log) : kind_(EventDataKind::kEventLog),
                                                 log_(&log) {}
  explicit EventDataView(const EventStream& stream)
      : kind_(EventDataKind::kEventStream), stream_(&stream) {}
  explicit EventDataView(const EventTable& table)
      : kind_(EventDataKind::kEventTable), table_(&table) {}
  explicit EventDataView(const LazyEventTable& table)
      : kind_(EventDataKind::kLazyEventTable), lazy_table_(&table) {}

  [[nodiscard]] EventDataKind kind() const noexcept { return kind_; }
  [[nodiscard]] const EventLog* log() const noexcept { return log_; }
  [[nodiscard]] const EventStream* stream() const noexcept { return stream_; }
  [[nodiscard]] const EventTable* table() const noexcept { return table_; }
  [[nodiscard]] const LazyEventTable* lazy_table() const noexcept {
    return lazy_table_;
  }

 private:
  EventDataKind kind_ = EventDataKind::kEventLog;
  const EventLog* log_ = nullptr;
  const EventStream* stream_ = nullptr;
  const EventTable* table_ = nullptr;
  const LazyEventTable* lazy_table_ = nullptr;
};

}  // namespace minera
