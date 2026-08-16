#include "minera/streaming_store.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

using StateRow = std::pair<std::string, std::string>;

void write_csv_field(std::ostream& out, std::string_view value) {
  const bool quoted = value.find_first_of(",\"\r\n") != std::string_view::npos;
  if (!quoted) {
    out << value;
    return;
  }
  out << '"';
  for (const char character : value) {
    if (character == '"') out << "\"\"";
    else out << character;
  }
  out << '"';
}

void write_header(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("cannot create streaming DFG state file");
  output << "case_id,activity\n";
  if (!output) throw std::runtime_error("cannot initialize streaming DFG state file");
}

std::vector<StateRow> read_state_rows(std::istream& input) {
  const std::string body((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  std::vector<StateRow> rows;
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;
  bool after_quote = false;
  bool field_start = true;

  const auto finish_row = [&]() {
    fields.push_back(std::move(field));
    field.clear();
    if (fields.size() != 2)
      throw std::invalid_argument("streaming DFG state row must have two fields");
    if (fields[0].empty() || fields[1].empty())
      throw std::invalid_argument("streaming DFG state row contains an empty field");
    rows.emplace_back(std::move(fields[0]), std::move(fields[1]));
    fields.clear();
    field_start = true;
    after_quote = false;
  };

  for (std::size_t i = 0; i < body.size(); ++i) {
    const char character = body[i];
    if (in_quotes) {
      if (character != '"') {
        field.push_back(character);
      } else if (i + 1 < body.size() && body[i + 1] == '"') {
        field.push_back('"');
        ++i;
      } else {
        in_quotes = false;
        after_quote = true;
      }
      continue;
    }

    if (after_quote) {
      if (character == ',') {
        fields.push_back(std::move(field));
        field.clear();
        field_start = true;
        after_quote = false;
      } else if (character == '\n' || character == '\r') {
        if (character == '\r' && i + 1 < body.size() && body[i + 1] == '\n')
          ++i;
        finish_row();
      } else {
        throw std::invalid_argument(
            "streaming DFG state has data after a quoted field");
      }
      continue;
    }

    if (character == '"') {
      if (!field_start)
        throw std::invalid_argument("streaming DFG state has an invalid quote");
      in_quotes = true;
      field_start = false;
    } else if (character == ',') {
      fields.push_back(std::move(field));
      field.clear();
      field_start = true;
    } else if (character == '\n' || character == '\r') {
      if (character == '\r' && i + 1 < body.size() && body[i + 1] == '\n')
        ++i;
      finish_row();
    } else {
      field.push_back(character);
      field_start = false;
    }
  }

  if (in_quotes)
    throw std::invalid_argument("streaming DFG state has an unterminated quote");
  if (after_quote || !fields.empty() || !field.empty()) finish_row();
  return rows;
}

}  // namespace

PersistentStreamingDfg::PersistentStreamingDfg(std::filesystem::path path,
                                               StreamingDfgOptions options)
    : path_(std::move(path)), discovery_(std::move(options)) {
  if (path_.empty())
    throw std::invalid_argument("streaming DFG state path must not be empty");

  std::error_code error;
  const bool exists = std::filesystem::exists(path_, error);
  if (error) throw std::runtime_error("cannot inspect streaming DFG state file");
  if (!exists) {
    write_header(path_);
    return;
  }

  const auto size = std::filesystem::file_size(path_, error);
  if (error) throw std::runtime_error("cannot inspect streaming DFG state file");
  if (size == 0) {
    write_header(path_);
    return;
  }

  std::ifstream input(path_, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open streaming DFG state file");
  std::string header;
  std::getline(input, header);
  if (!header.empty() && header.back() == '\r') header.pop_back();
  if (header != "case_id,activity")
    throw std::invalid_argument("invalid streaming DFG state file header");

  for (const auto& [case_id, activity] : read_state_rows(input))
    if (!discovery_.receive(case_id, activity))
      throw std::invalid_argument("streaming DFG state contains an invalid event");
}

void PersistentStreamingDfg::append(std::string_view case_id,
                                    std::string_view activity) {
  std::ofstream output(path_, std::ios::binary | std::ios::app);
  if (!output) throw std::runtime_error("cannot append streaming DFG state file");
  write_csv_field(output, case_id);
  output << ',';
  write_csv_field(output, activity);
  output << '\n';
  if (!output) throw std::runtime_error("cannot append streaming DFG state file");
}

bool PersistentStreamingDfg::receive(std::string_view case_id,
                                     std::string_view activity) {
  if (case_id.empty() || activity.empty()) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  append(case_id, activity);
  return discovery_.receive(case_id, activity);
}

bool PersistentStreamingDfg::receive(const Event& event,
                                     std::string_view case_id) {
  return receive(case_id, event.activity);
}

StreamingDfgSnapshot PersistentStreamingDfg::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return discovery_.snapshot();
}

void PersistentStreamingDfg::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  discovery_.reset();
  write_header(path_);
}

}  // namespace minera
