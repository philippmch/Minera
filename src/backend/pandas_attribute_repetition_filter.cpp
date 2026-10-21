#include "minera/pandas_attribute_repetition_filter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>
#include <unordered_map>

#include "minera/timestamp.hpp"
#include "root_cause_trace_key.hpp"

namespace minera {
namespace {

[[noreturn]] void error(std::string type, std::string message) {
  throw PandasAttributeRepetitionError(std::move(type), std::move(message));
}

std::string key_text(const PandasAttributeRepetitionScalar& key) {
  return std::visit([](const auto& value) -> std::string {
    using T=std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T,std::nullptr_t>) return "None";
    else if constexpr (std::is_same_v<T,bool>) return value ? "True" : "False";
    else if constexpr (std::is_same_v<T,std::string>) return "'"+value+"'";
    else if constexpr (std::is_same_v<T,TokenInteger>) return value.decimal();
    else return std::to_string(value);
  },key);
}

std::size_t column(const EventTable& source,
                     const PandasAttributeRepetitionScalar& key, bool grouping) {
  if (const auto* name=std::get_if<std::string>(&key)) {
    const auto found=std::find(source.columns.begin(),source.columns.end(),*name);
    if (found!=source.columns.end()) return static_cast<std::size_t>(found-source.columns.begin());
  } else if (grouping && std::holds_alternative<std::nullptr_t>(key)) {
    error("TypeError","You have to supply one of 'by' and 'level'");
  }
  error("KeyError",key_text(key));
}

bool date(const EventAttribute& value) {
  return value.kind==AttributeKind::kDate || std::holds_alternative<NativeDate>(value.typed_value);
}

std::optional<ParsedExactTimestamp> instant(const EventAttribute& value) {
  const auto* stored=std::get_if<NativeDate>(&value.typed_value);
  return parse_timestamp_exact(stored ? stored->value : value.value);
}

bool missing(const EventAttribute& value) {
  if (value.is_null || is_native_na_attribute(value)) return true;
  if (const auto* real=std::get_if<double>(&value.typed_value)) return std::isnan(*real);
  if (value.kind==AttributeKind::kFloat && std::holds_alternative<std::monostate>(value.typed_value))
    return std::isnan(native_attribute_number(value));
  return date(value) && !instant(value);
}

bool same_instant(const ParsedExactTimestamp& a, const ParsedExactTimestamp& b) {
  return a.timezone_aware==b.timezone_aware && a.seconds==b.seconds && a.nanoseconds==b.nanoseconds;
}

bool scalar_equal(const EventAttribute& left, const EventAttribute& right,
                    bool datetime_column) {
  if (missing(left) || missing(right)) return false;
  if (datetime_column) {
    const auto a=instant(left);
    if (!date(right) && (right.kind!=AttributeKind::kString ||
                        right.container!=AttributeContainerKind::kScalar)) return false;
    const auto b=instant(right);
    return a && b && same_instant(*a,*b);
  }
  if (date(left) || date(right)) {
    if (!date(left) || !date(right)) return false;
    const auto a=instant(left),b=instant(right);
    return a && b && same_instant(*a,*b);
  }
  if (left.container!=right.container) return false;
  if (!root_cause_key_is_hashable(left) || !root_cause_key_is_hashable(right)) {
    // Native scalar source cells do not become equal to mutable containers.
    if (left.container==AttributeContainerKind::kScalar || right.container==AttributeContainerKind::kScalar)
      return false;
    error("TypeError","unsupported mutable dataframe comparison");
  }
  return root_cause_scalar_keys_equal(left,right);
}

std::string group_key(const EventAttribute& value) {
  if (!root_cause_key_is_hashable(value)) {
    const auto kind=value.container==AttributeContainerKind::kList ? "list"
        : value.container==AttributeContainerKind::kContainer ? "dict" : "set";
    error("TypeError",std::string("unhashable type: '")+kind+"'");
  }
  if (date(value)) {
    const auto parsed=instant(value);
    if (!parsed) return "missing";
    return std::string(parsed->timezone_aware ? "DA" : "DN")+
        std::to_string(parsed->seconds)+":"+std::to_string(parsed->nanoseconds);
  }
  return root_cause_scalar_key_fingerprint(value);
}

bool bound_accepts(std::uint64_t count, const PandasAttributeRepetitionScalar& bound,
                    bool lower) {
  return std::visit([&](const auto& value) -> bool {
    using T=std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T,std::nullptr_t>) return false;
    else if constexpr (std::is_same_v<T,std::string>) {
      error("TypeError","Invalid comparison between dtype=int64 and str");
    } else if constexpr (std::is_same_v<T,TokenInteger>) {
      return lower ? value<=TokenInteger(count) : TokenInteger(count)<=value;
    } else if constexpr (std::is_same_v<T,bool>) {
      return lower ? std::uint64_t(value)<=count : count<=std::uint64_t(value);
    } else {
      if (std::isnan(value)) return false;
      if (lower) return value<=0 || (value<0x1p64 && count>=static_cast<std::uint64_t>(std::ceil(value)));
      return value>=0 && (value>=0x1p64 || count<=static_cast<std::uint64_t>(std::floor(value)));
    }
  },bound);
}

bool arithmetic_index(const std::vector<EventTableIndexLabel>& labels) {
  std::vector<TokenInteger> numbers;
  numbers.reserve(labels.size());
  for (const auto& label:labels) {
    const auto* value=std::get_if<EventAttribute>(&label.value);
    if (!value || value->is_null) return false;
    if (const auto* number=std::get_if<std::int64_t>(&value->typed_value)) numbers.emplace_back(*number);
    else if (const auto* number=std::get_if<std::uint64_t>(&value->typed_value)) numbers.emplace_back(*number);
    else return false;
  }
  if (numbers.size()<2) return true;
  const TokenInteger step=numbers[1]-numbers[0];
  if (step==0) return false;
  for (std::size_t i=2;i<numbers.size();++i)
    if (numbers[i]-numbers[i-1]!=step) return false;
  return true;
}

}  // namespace

EventTable filter_pandas_attribute_value_repetition(
    const EventTable& source, const EventAttribute& target,
    const PandasAttributeRepetitionOptions& options) {
  source.validate();
  const auto selected=column(source,options.attribute_key,false);
  const bool vector_query=target.container==AttributeContainerKind::kList;
  if (vector_query && target.children.size()!=source.rows.size())
    error("ValueError","('Lengths must match to compare', ("+std::to_string(source.rows.size())+
        ",), ("+std::to_string(target.children.size())+",))");
  const auto grouped=column(source,options.case_id_key,true);
  const bool datetime_column=!source.dtypes.empty() && source.dtypes[selected].starts_with("datetime64");
  std::unordered_map<std::string,std::uint64_t> counts;
  for (std::size_t i=0;i<source.rows.size();++i) {
    const auto& candidate=source.rows[i][selected];
    const auto& wanted=vector_query ? target.children[i] : target;
    if (!candidate || !scalar_equal(*candidate,wanted,datetime_column)) continue;
    const auto& key=source.rows[i][grouped];
    if (!key || missing(*key)) continue;
    ++counts[group_key(*key)];
  }
  // Vectorized dataframe comparisons evaluate both bound operands.
  if (std::holds_alternative<std::string>(options.minimum) ||
      std::holds_alternative<std::string>(options.maximum))
    error("TypeError","Invalid comparison between dtype=int64 and str");
  std::unordered_map<std::string,bool> eligible;
  for (const auto& [key,count]:counts) {
    const bool lower=bound_accepts(count,options.minimum,true);
    const bool upper=bound_accepts(count,options.maximum,false);
    eligible.emplace(key,lower && upper);
  }
  EventTable result=source;
  result.rows.clear();
  if (result.index) result.index->labels.clear();
  std::vector<std::size_t> positions;
  for (std::size_t i=0;i<source.rows.size();++i) {
    const auto& key=source.rows[i][grouped];
    if (!key || missing(*key)) continue;
    // Unmatched mutable case values are irrelevant to grouping and isin.
    if (!root_cause_key_is_hashable(*key)) continue;
    const auto found=eligible.find(group_key(*key));
    if (found==eligible.end() || !found->second) continue;
    result.rows.push_back(source.rows[i]);
    positions.push_back(i);
    if (result.index) result.index->labels.push_back(source.index->labels[i]);
  }
  if (!result.index && positions.size()!=source.rows.size()) {
    EventTableIndex index;
    for (const auto position:positions) {
      EventAttribute value{"",std::to_string(position),AttributeKind::kInt};
      value.typed_value=static_cast<std::uint64_t>(position);
      index.labels.push_back({std::move(value)});
    }
    result.index=std::move(index);
  }
  if (result.index && result.index->kind=="RangeIndex" && !arithmetic_index(result.index->labels))
    result.index->kind="Index";
  return result;
}

}  // namespace minera
