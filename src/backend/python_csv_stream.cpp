#include "minera/python_csv_stream.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace minera {

const std::optional<std::string>* PythonCsvRecord::find(std::string_view key) const {
  for (const auto& [name, value] : fields)
    if (name == key) return &value;
  return nullptr;
}

namespace {

// Text-mode reading: CRLF and lone CR become LF before the csv module sees them.
std::string universal_newlines(std::string text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r') {
      out.push_back('\n');
      if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
    } else {
      out.push_back(text[i]);
    }
  }
  return out;
}

// csv.reader with the excel dialect over newline-terminated lines.
std::vector<std::vector<std::string>> parse_rows(const std::string& text) {
  enum class State { kStartRecord, kStartField, kInField, kInQuoted, kQuoteInQuoted };
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  State state = State::kStartRecord;
  const auto save = [&] { row.push_back(std::move(field)); field.clear(); };
  const auto end_record = [&] { save(); rows.push_back(std::move(row)); row.clear(); state = State::kStartRecord; };
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\0') throw std::runtime_error("line contains NUL");
    switch (state) {
      case State::kStartRecord:
        if (c == '\n') { rows.emplace_back(); break; }
        state = State::kStartField;
        [[fallthrough]];
      case State::kStartField:
        if (c == '\n') end_record();
        else if (c == '"') state = State::kInQuoted;
        else if (c == ',') save();
        else { field.push_back(c); state = State::kInField; }
        break;
      case State::kInField:
        if (c == '\n') end_record();
        else if (c == ',') { save(); state = State::kStartField; }
        else field.push_back(c);
        break;
      case State::kInQuoted:
        if (c == '"') state = State::kQuoteInQuoted;
        else field.push_back(c);
        break;
      case State::kQuoteInQuoted:
        if (c == '"') { field.push_back('"'); state = State::kInQuoted; }
        else if (c == ',') { save(); state = State::kStartField; }
        else if (c == '\n') end_record();
        else { field.push_back(c); state = State::kInField; }
        break;
    }
  }
  // A last line without its newline still ends its record; an unterminated
  // quoted field is saved as read (non-strict dialect).
  if (state != State::kStartRecord) end_record();
  return rows;
}

}  // namespace

std::vector<PythonCsvRecord> read_python_csv_event_stream(std::istream& input) {
  const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  const auto rows = parse_rows(universal_newlines(bytes));
  std::vector<PythonCsvRecord> records;
  if (rows.empty()) return records;
  const auto& header = rows.front();
  for (std::size_t r = 1; r < rows.size(); ++r) {
    const auto& row = rows[r];
    if (row.empty()) continue;  // DictReader skips blank lines
    PythonCsvRecord record;
    const auto set = [&record](const std::string& key, std::optional<std::string> value) {
      for (auto& [name, current] : record.fields)
        if (name == key) { current = std::move(value); return; }
      record.fields.emplace_back(key, std::move(value));
    };
    for (std::size_t i = 0; i < header.size() && i < row.size(); ++i) set(header[i], row[i]);
    if (row.size() > header.size())
      record.extras.assign(row.begin() + static_cast<std::ptrdiff_t>(header.size()), row.end());
    for (std::size_t i = row.size(); i < header.size(); ++i) set(header[i], std::nullopt);
    records.push_back(std::move(record));
  }
  return records;
}

std::vector<PythonCsvRecord> read_python_csv_event_stream(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open CSV event stream: " + path.string());
  return read_python_csv_event_stream(input);
}

Event python_csv_record_event(const PythonCsvRecord& record, std::size_t input_order) {
  Event event;
  event.input_order = input_order;
  for (const auto& [key, value] : record.fields) {
    EventAttribute attribute{key, value.value_or(std::string{}), AttributeKind::kString};
    attribute.is_null = !value.has_value();
    event.attributes.push_back(std::move(attribute));
  }
  return event;
}

std::optional<std::string> python_csv_case_key(const PythonCsvRecord& record,
                                               std::string_view column) {
  const auto* value = record.find(column);
  if (!value) return std::nullopt;
  // The reference keys cases by their text, so a null cell joins "None".
  return value->has_value() ? **value : std::string("None");
}

}  // namespace minera
