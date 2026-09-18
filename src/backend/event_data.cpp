#include "minera/event_data.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
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

void validate_index(const EventTableIndex& index,
                    std::optional<std::size_t> row_count) {
  if (index.kind.empty())
    throw std::invalid_argument("event table: index kind must not be empty");
  if (index.dtype.empty())
    throw std::invalid_argument("event table: index dtype must not be empty");
  if (row_count && index.labels.size() != *row_count)
    throw std::invalid_argument(
        "event table: index label count does not match row count");

  if (index.kind == "MultiIndex") {
    if (index.names.empty())
      throw std::invalid_argument(
          "event table: MultiIndex must have at least one name");
    for (const auto& label : index.labels) {
      const auto* tuple =
          std::get_if<std::vector<EventTableIndexLabel>>(&label.value);
      if (!tuple)
        throw std::invalid_argument(
            "event table: MultiIndex labels must be tuples");
      if (tuple->size() != index.names.size())
        throw std::invalid_argument(
            "event table: MultiIndex label arity does not match names");
    }
    return;
  }

  if (index.names.size() != 1)
    throw std::invalid_argument(
        "event table: ordinary index must have exactly one name");
}

bool category_attribute_is_nan(const EventAttribute& attribute) {
  if (const auto* value = std::get_if<double>(&attribute.typed_value))
    return std::isnan(*value);
  if (attribute.kind != AttributeKind::kFloat || attribute.value.empty())
    return false;
  char* end = nullptr;
  const double parsed = std::strtod(attribute.value.c_str(), &end);
  return end == attribute.value.c_str() + attribute.value.size() &&
         std::isnan(parsed);
}

void validate_category_attribute(const EventAttribute& attribute,
                                 std::string_view path,
                                 bool top_level = true) {
  if (top_level && (attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull))
    throw std::invalid_argument("event table: categorical " + std::string(path) +
                                " must not be null");
  if (top_level && category_attribute_is_nan(attribute))
    throw std::invalid_argument("event table: categorical " + std::string(path) +
                                " must not be NaN");

  if (attribute.container == AttributeContainerKind::kScalar) {
    if (!attribute.children.empty() || attribute.scalar_wrapper ||
        attribute.scalar_children_list)
      throw std::invalid_argument(
          "event table: categorical values must be scalars or recursive tuples");
    return;
  }
  if (attribute.container != AttributeContainerKind::kTuple ||
      attribute.scalar_wrapper || attribute.scalar_children_list)
    throw std::invalid_argument(
        "event table: categorical values must use scalar or tuple containers");
  for (std::size_t index = 0; index < attribute.children.size(); ++index) {
    validate_category_attribute(
        attribute.children[index],
        std::string(path) + " tuple element " + std::to_string(index), false);
  }
}

using NumericCategoryValue =
    std::variant<std::int64_t, std::uint64_t, double>;

std::optional<NumericCategoryValue> category_numeric_value(
    const EventAttribute& attribute) {
  if (attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull ||
      attribute.container != AttributeContainerKind::kScalar ||
      attribute.scalar_wrapper || attribute.scalar_children_list ||
      !attribute.children.empty())
    return std::nullopt;
  if (const auto* value = std::get_if<std::int64_t>(&attribute.typed_value))
    return *value;
  if (const auto* value = std::get_if<std::uint64_t>(&attribute.typed_value))
    return *value;
  if (const auto* value = std::get_if<double>(&attribute.typed_value))
    return *value;
  if (const auto* value = std::get_if<bool>(&attribute.typed_value))
    return static_cast<std::int64_t>(*value ? 1 : 0);

  if (attribute.kind == AttributeKind::kBoolean) {
    if (attribute.value == "true" || attribute.value == "1")
      return std::int64_t{1};
    if (attribute.value == "false" || attribute.value == "0")
      return std::int64_t{0};
    return std::nullopt;
  }
  if (attribute.kind == AttributeKind::kInt) {
    std::string_view text = attribute.value;
    if (text.starts_with('+')) text.remove_prefix(1);
    if (text.empty()) return std::nullopt;
    std::int64_t signed_value = 0;
    const auto signed_result = std::from_chars(
        text.data(), text.data() + text.size(), signed_value, 10);
    if (signed_result.ec == std::errc{} &&
        signed_result.ptr == text.data() + text.size())
      return signed_value;
    std::uint64_t unsigned_value = 0;
    const auto unsigned_result = std::from_chars(
        text.data(), text.data() + text.size(), unsigned_value, 10);
    if (unsigned_result.ec == std::errc{} &&
        unsigned_result.ptr == text.data() + text.size())
      return unsigned_value;
    return std::nullopt;
  }
  if (attribute.kind == AttributeKind::kFloat) {
    if (attribute.value.empty()) return std::nullopt;
    char* end = nullptr;
    const double parsed = std::strtod(attribute.value.c_str(), &end);
    if (end == attribute.value.c_str() ||
        end != attribute.value.c_str() + attribute.value.size())
      return std::nullopt;
    return parsed;
  }
  return std::nullopt;
}

bool double_equals_signed(double real, std::int64_t integer) {
  if (!std::isfinite(real) || std::trunc(real) != real ||
      real < -0x1p63 || real >= 0x1p63)
    return false;
  return static_cast<std::int64_t>(real) == integer;
}

bool double_equals_unsigned(double real, std::uint64_t integer) {
  if (!std::isfinite(real) || std::trunc(real) != real || real < 0 ||
      real >= 0x1p64)
    return false;
  return static_cast<std::uint64_t>(real) == integer;
}

bool numeric_category_equal(const NumericCategoryValue& left,
                            const NumericCategoryValue& right) {
  if (const auto* left_signed = std::get_if<std::int64_t>(&left)) {
    if (const auto* right_signed = std::get_if<std::int64_t>(&right))
      return *left_signed == *right_signed;
    if (const auto* right_unsigned = std::get_if<std::uint64_t>(&right))
      return *left_signed >= 0 &&
             static_cast<std::uint64_t>(*left_signed) == *right_unsigned;
    return double_equals_signed(std::get<double>(right), *left_signed);
  }
  if (const auto* left_unsigned = std::get_if<std::uint64_t>(&left)) {
    if (const auto* right_unsigned = std::get_if<std::uint64_t>(&right))
      return *left_unsigned == *right_unsigned;
    if (const auto* right_signed = std::get_if<std::int64_t>(&right))
      return *right_signed >= 0 &&
             *left_unsigned == static_cast<std::uint64_t>(*right_signed);
    return double_equals_unsigned(std::get<double>(right), *left_unsigned);
  }
  const double left_real = std::get<double>(left);
  if (const auto* right_real = std::get_if<double>(&right)) {
    if (std::isnan(left_real) || std::isnan(*right_real))
      return std::isnan(left_real) && std::isnan(*right_real);
    return left_real == *right_real;
  }
  if (const auto* right_signed = std::get_if<std::int64_t>(&right))
    return double_equals_signed(left_real, *right_signed);
  return double_equals_unsigned(left_real, std::get<std::uint64_t>(right));
}

void append_category_key_part(std::string& key, std::string_view value) {
  key += std::to_string(value.size());
  key.push_back(':');
  key.append(value);
  key.push_back(';');
}

std::string numeric_category_key(const NumericCategoryValue& value) {
  if (const auto* signed_value = std::get_if<std::int64_t>(&value))
    return "integer:" + std::to_string(*signed_value);
  if (const auto* unsigned_value = std::get_if<std::uint64_t>(&value))
    return "integer:" + std::to_string(*unsigned_value);
  const double real = std::get<double>(value);
  if (std::isnan(real)) return "real:nan";
  if (std::isinf(real)) return real < 0 ? "real:-inf" : "real:inf";
  if (std::trunc(real) == real) {
    if (real >= -0x1p63 && real < 0x1p63)
      return "integer:" + std::to_string(static_cast<std::int64_t>(real));
    if (real >= 0 && real < 0x1p64)
      return "integer:" + std::to_string(static_cast<std::uint64_t>(real));
  }
  char buffer[64];
  const auto converted = std::to_chars(
      buffer, buffer + sizeof(buffer), real, std::chars_format::general,
      std::numeric_limits<double>::max_digits10);
  if (converted.ec == std::errc{})
    return "real:" + std::string(buffer, converted.ptr);
  return "real:" + std::to_string(real);
}

void append_typed_category_key(std::string& key,
                               const NativeAttributeValue& value) {
  if (std::holds_alternative<std::monostate>(value)) {
    key += "typed:none;";
  } else if (const auto* item = std::get_if<std::int64_t>(&value)) {
    append_category_key_part(key, "int:" + std::to_string(*item));
  } else if (const auto* item = std::get_if<std::uint64_t>(&value)) {
    append_category_key_part(key, "uint:" + std::to_string(*item));
  } else if (const auto* item = std::get_if<double>(&value)) {
    if (std::isnan(*item)) append_category_key_part(key, "float:nan");
    else if (std::isinf(*item))
      append_category_key_part(key, *item < 0 ? "float:-inf" : "float:inf");
    else if (*item == 0.0)
      append_category_key_part(key, "float:0");
    else {
      char buffer[64];
      const auto converted = std::to_chars(
          buffer, buffer + sizeof(buffer), *item,
          std::chars_format::general,
          std::numeric_limits<double>::max_digits10);
      append_category_key_part(
          key, converted.ec == std::errc{}
                   ? std::string_view(buffer, converted.ptr)
                   : std::string_view("invalid"));
    }
  } else if (const auto* item = std::get_if<bool>(&value)) {
    append_category_key_part(key, *item ? "bool:true" : "bool:false");
  } else if (const auto* item = std::get_if<NativeDate>(&value)) {
    append_category_key_part(key, "date:" + item->value);
  }
}

std::string category_semantic_key(const EventAttribute& attribute) {
  const bool is_null = attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull;
  std::string key;
  if (is_null) {
    key = "null:" + std::to_string(static_cast<int>(attribute.container)) +
        ":" + std::to_string(attribute.children.size());
    return key;
  }
  if (attribute.container == AttributeContainerKind::kScalar) {
    if (const auto numeric = category_numeric_value(attribute))
      return "numeric:" + numeric_category_key(*numeric);
    key = "scalar:" + std::to_string(static_cast<int>(attribute.kind)) + ":";
    append_typed_category_key(key, attribute.typed_value);
    // A native value is authoritative.  The text projection can be stale
    // after an adapter normalizes a typed date, while semantic equality only
    // compares the native value in that case.
    if (std::holds_alternative<std::monostate>(attribute.typed_value))
      append_category_key_part(key, attribute.value);
    return key;
  }
  key = "container:" + std::to_string(static_cast<int>(attribute.container)) +
      ":" + std::to_string(static_cast<int>(attribute.kind)) + ":" +
      std::to_string(attribute.children.size()) + ":";
  append_typed_category_key(key, attribute.typed_value);
  for (const auto& child : attribute.children)
    append_category_key_part(key, category_semantic_key(child));
  return key;
}

bool semantic_category_equal(const EventAttribute& left,
                             const EventAttribute& right) {
  // Category identity intentionally ignores keys and adapter role/state
  // fields. Those fields describe where a value came from, not its category
  // value. Pandas compares bool, signed/unsigned integer, and float scalar
  // categories by numeric value, while retaining None/NaN distinction.
  const bool left_null = left.is_null ||
      left.first_class_state == FirstClassAttributeState::kNull;
  const bool right_null = right.is_null ||
      right.first_class_state == FirstClassAttributeState::kNull;
  if (left_null != right_null || left.container != right.container ||
      left.children.size() != right.children.size())
    return false;
  if (left.container == AttributeContainerKind::kScalar && !left_null) {
    const auto left_numeric = category_numeric_value(left);
    const auto right_numeric = category_numeric_value(right);
    if (left_numeric || right_numeric) {
      return left_numeric && right_numeric &&
             numeric_category_equal(*left_numeric, *right_numeric);
    }
  }
  if (left_null) return true;
  if (left.kind != right.kind) return false;
  const bool left_typed =
      !std::holds_alternative<std::monostate>(left.typed_value);
  const bool right_typed =
      !std::holds_alternative<std::monostate>(right.typed_value);
  if (left_typed != right_typed) return false;
  if (left_typed) {
    if (left.typed_value != right.typed_value) return false;
  } else if (left.container == AttributeContainerKind::kScalar &&
             left.value != right.value) {
    return false;
  }
  for (std::size_t index = 0; index < left.children.size(); ++index) {
    if (!semantic_category_equal(left.children[index], right.children[index]))
      return false;
  }
  return true;
}

void validate_table_metadata(const std::vector<std::string>& columns,
                             const std::vector<std::string>& dtypes,
                             const std::optional<EventTableIndex>& index,
                             const std::vector<std::optional<
                                 EventTableCategoricalDtype>>& categorical_dtypes,
                             std::optional<std::size_t> row_count) {
  if (!dtypes.empty()) {
    if (dtypes.size() != columns.size())
      throw std::invalid_argument(
          "event table: dtype count does not match column count");
    for (const auto& dtype : dtypes) {
      if (dtype.empty())
        throw std::invalid_argument(
            "event table: declared dtypes must not be empty");
    }
  }
  if (index) validate_index(*index, row_count);
  if (!categorical_dtypes.empty()) {
    if (categorical_dtypes.size() != columns.size())
      throw std::invalid_argument(
          "event table: categorical dtype count does not match column count");
    if (dtypes.empty())
      throw std::invalid_argument(
          "event table: categorical metadata requires declared dtypes");
    for (std::size_t column = 0; column < categorical_dtypes.size(); ++column) {
      if (!categorical_dtypes[column]) continue;
      if (dtypes[column] != "category")
        throw std::invalid_argument(
            "event table: categorical descriptor requires dtype category");
      const auto& descriptor = *categorical_dtypes[column];
      if (descriptor.categories_dtype.empty())
        throw std::invalid_argument(
            "event table: categorical categories dtype must not be empty");
      std::unordered_map<std::string, std::vector<std::size_t>> category_buckets;
      category_buckets.reserve(descriptor.categories.size());
      for (std::size_t category = 0; category < descriptor.categories.size();
           ++category) {
        validate_category_attribute(
            descriptor.categories[category],
            "category " + std::to_string(category) + " in column " +
                columns[column]);
        auto& bucket = category_buckets[category_semantic_key(
            descriptor.categories[category])];
        for (const auto prior : bucket) {
          if (semantic_category_equal(descriptor.categories[prior],
                                      descriptor.categories[category]))
            throw std::invalid_argument(
                "event table: categorical descriptor contains duplicate category");
        }
        bucket.push_back(category);
      }
    }
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
  validate_table_metadata(columns, dtypes, index, categorical_dtypes,
                          rows.size());
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
                               std::optional<std::size_t> row_count,
                               std::vector<std::string> dtypes,
                               std::optional<EventTableIndex> index,
                               std::vector<std::optional<
                                   EventTableCategoricalDtype>> categorical_dtypes)
    : columns_(std::move(columns)),
      scanner_(std::move(scanner)),
      attributes_(std::move(attributes)),
      metadata_(std::move(metadata)),
      row_count_(row_count),
      dtypes_(std::move(dtypes)),
      index_(std::move(index)),
      categorical_dtypes_(std::move(categorical_dtypes)) {
  validate_columns(columns_);
  validate_table_metadata(columns_, dtypes_, index_, categorical_dtypes_,
                          row_count_);
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
  if (index_ && count != index_->labels.size())
    throw std::invalid_argument(
        "lazy event table: index label count does not match scanned rows");
}

EventTable LazyEventTable::materialize(
    std::optional<std::size_t> max_rows) const {
  EventTable table;
  table.columns = columns_;
  table.attributes = attributes_;
  table.metadata = metadata_;
  table.dtypes = dtypes_;
  table.index = index_;
  table.categorical_dtypes = categorical_dtypes_;
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
      table->attributes, table->metadata, table->rows.size(), table->dtypes,
      table->index, table->categorical_dtypes);
}

}  // namespace minera
