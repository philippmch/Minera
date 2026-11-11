#include "minera/local_diagnostics.hpp"

#include "frame_cells.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {
constexpr std::size_t kHardGroups = 10000;
constexpr std::size_t kHardResources = 100000;
constexpr std::size_t kHardRows = 1000000;

std::string value_for(const Event& e, const std::string& key, bool resource) {
  if (key.empty()) return resource ? e.resource : e.activity;
  if (resource && (key == "org:resource" || key == "resource")) return e.resource;
  if (!resource && (key == "concept:name" || key == "activity")) return e.activity;
  const auto* v = e.attribute(key);
  return v ? *v : std::string{};
}
bool nonblank(const std::string& s) {
  return std::any_of(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); });
}

void quote(std::ostringstream& out, const std::string& s) {
  out << '"';
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') out << '\\';
    if (c < 0x20) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<int>(c) << std::dec << std::setfill(' ');
    else out << c;
  }
  out << '"';
}

template <typename Map>
void emit_double_map(std::ostringstream& out, const Map& map) {
  out << '{'; bool first_group = true;
  for (const auto& [g, values] : map) {
    if (!first_group) out << ','; first_group = false; quote(out, g); out << ':' << '{';
    bool first = true;
    for (const auto& [k, v] : values) { if (!first) out << ','; first = false; quote(out, k); out << ':' << std::setprecision(17) << v; }
    out << '}';
  }
  out << '}';
}
// Python's repr() of a str, as a KeyError prints its key.
std::string python_repr(const std::string& text) {
  const bool double_quote = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
  const char quote = double_quote ? '"' : '\'';
  std::string out(1, quote);
  for (const unsigned char c : text) {
    if (c == '\\') out += "\\\\";
    else if (c == static_cast<unsigned char>(quote)) { out += '\\'; out += quote; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c < 0x20 || c == 0x7f) {
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "\\x%02x", c);
      out += buffer;
    } else out += static_cast<char>(c);
  }
  return out + quote;
}

[[noreturn]] void zero_division() {
  throw std::invalid_argument("ZeroDivisionError: float division by zero");
}

// repr() of a date cell: a datetime.datetime in its own UTC offset in an
// XES object column, a Timestamp in a datetime column (UTC).
std::string date_repr(const python::Scalar& date, bool object_column) {
  const auto text = python::str(date);  // "YYYY-MM-DD HH:MM:SS[.ffffff][+HH:MM]"
  if (!object_column) {
    auto shown = text;
    if (shown.size() >= 6 && (shown[shown.size() - 6] == '+' || shown[shown.size() - 6] == '-') &&
        shown[shown.size() - 3] == ':')
      shown.erase(shown.size() - 3, 1);
    return "Timestamp('" + shown + "'" + (date.date.timezone_aware ? ", tz='UTC'" : "") + ")";
  }
  const auto number = [&](std::size_t at, std::size_t width) { return std::stoi(text.substr(at, width)); };
  std::string out = "datetime.datetime(" + std::to_string(number(0, 4)) + ", " + std::to_string(number(5, 2)) + ", " +
                    std::to_string(number(8, 2)) + ", " + std::to_string(number(11, 2)) + ", " +
                    std::to_string(number(14, 2));
  const int second = number(17, 2);
  std::size_t position = 19;
  int micro = 0;
  if (position < text.size() && text[position] == '.') {
    std::string digits;
    ++position;
    while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) digits += text[position++];
    digits.resize(6, '0');
    micro = std::stoi(digits.substr(0, 6));
  }
  if (second || micro) out += ", " + std::to_string(second);
  if (micro) out += ", " + std::to_string(micro);
  if (position < text.size()) {
    const int sign = text[position] == '-' ? -1 : 1;
    const int seconds = sign * (number(position + 1, 2) * 60 + number(position + 4, 2)) * 60;
    if (seconds == 0) out += ", tzinfo=datetime.timezone.utc";
    else
      out += ", tzinfo=datetime.timezone(datetime.timedelta(" +
             (seconds < 0 ? "days=-1, seconds=" + std::to_string(86400 + seconds) : "seconds=" + std::to_string(seconds)) +
             "))";
  }
  return out + ")";
}

// A groupby's TypeError: an XES object column's dates are datetime.datetime
// cells (a datetime column never fails to sort).
[[noreturn]] void sort_error(const python::TypeError& error) {
  std::string message = error.what();
  for (std::size_t at; (at = message.find("'Timestamp'")) != std::string::npos;)
    message.replace(at, 11, "'datetime.datetime'");
  throw std::invalid_argument("TypeError: " + message);
}

// pm4py's local diagnostics on a DataFrame (CSV and XES uploads), which runs
// its pandas implementation (observed black-box, 3,000 random frames against
// a model of it): keys are frame columns (activity, then resource, then group
// raise KeyError), cells are typed values, and
// - C[r][a] counts the rows of resource r and activity a whose group cell is
//   no NaN (a pair seen only with NaN groups counts 0); rows without a
//   resource or an activity are left out;
// - group-attribute mode takes a group's members from the rows with that
//   group value and a resource, in the order of the groupby [resource,
//   group] (the groups by their first member, members by resource; round
//   BB: the KeyError below names the first such member); explicit groups list
//   their members (repeats once), and a member the resource column does not
//   hold raises KeyError;
// - focus[g][a] = sum of C[r][a] over g's members / sum over all resources,
//   stake[g][a] = the same / g's total, both ZeroDivisionError on a zero
//   divisor;
// - coverage[g][r] is, with a group attribute, r's share of the rows tagged
//   with g (rows with an activity), and with explicit groups r's share of
//   the members' rows (activity or not);
// - contribution[g][r] = C[r] with its zero counts, KeyError for a member
//   with no row that has an activity.
//
// pandas reads a list of two names as the row labels of a two-row frame
// when one of them is no column (row i is keyed by the i-th name), so on a
// two-row upload without the activity or resource column (round AZ, seeds
// 231, 301 and 324 of round AY; a model of it matched pm4py on 3,700 random
// two-row frames):
// - the pair groupby [activity, resource] keys row 0 by the activity name
//   and row 1 by the resource name; each label reads activity = label[0]
//   and resource = label[1] (IndexError for a name shorter than two
//   characters), in sorted label order, a later label of the same pair
//   replacing the earlier count; the counts are the rows' present group
//   cells, read after the group column's KeyError;
// - with a group attribute, a resource column means the coverage count
//   reads the missing activity column (KeyError); without one the
//   membership groupby [resource, group] keys row 0 by the resource name
//   and row 1 by the group name (resource = label[0], group = label[1]),
//   after the activity column's KeyError, and coverage counts the rows'
//   present activity cells per label, again replaced per pair;
// - explicit groups read the resource column (KeyError) as before.
LocalDiagnosticsResult frame_local_diagnostics(const EventLog& input,
                                               const LocalDiagnosticsOptions& o) {
  const auto frame = dataframe_frame_event_log(input);
  const auto scope = FrameKeyScope::kDataFrame;
  const auto keys = frame_keys(frame, scope);
  const std::string resource_key = o.resource_attribute.empty() ? "org:resource" : o.resource_attribute;
  const std::string activity_key = o.activity_attribute.empty() ? "concept:name" : o.activity_attribute;
  const std::string& group_key = o.group_attribute;
  std::size_t frame_rows = 0;
  for (const auto& trace : frame.traces) frame_rows += trace.events.size();
  const bool pair_labels = pandas_row_label_keys(frame_rows, keys.contains(activity_key) && keys.contains(resource_key));
  // The activity and resource columns are read first, and their groupby
  // sorts before the group column is read.
  if (!pair_labels)
    for (const auto* key : {&activity_key, &resource_key})
      if (!keys.contains(*key)) throw std::invalid_argument("KeyError: " + python_repr(*key));

  LocalDiagnosticsResult result;
  result.diagnostics.trace_count = input.trace_count();
  result.diagnostics.event_count = input.event_count();
  // Resources, activities and groups are separate dicts, keyed by Python
  // identity (1, 1.0 and True are one key) and named by str() of the first.
  struct Names {
    std::map<std::string, std::string> name;
    std::map<std::string, python::Scalar> dates;
    std::size_t size() const { return name.size(); }
    const std::string& add(const python::Scalar& value) {
      const auto& id = name.try_emplace(python::identity(value), python::str(value)).first->first;
      if (value.kind == python::Scalar::Kind::kDate) dates.try_emplace(id, value);
      return id;
    }
    // KeyError's repr of the key: text quoted, numbers as printed, dates as
    // their datetime.datetime or Timestamp.
    std::string repr(const std::string& id, bool object_column) const {
      if (const auto it = dates.find(id); it != dates.end()) return date_repr(it->second, object_column);
      return id.starts_with("S") ? python_repr(name.at(id)) : name.at(id);
    }
  };
  Names resources, activities, groups;
  struct Row {
    const std::string* resource = nullptr;
    const std::string* activity = nullptr;
    const std::string* group = nullptr;
  };
  std::vector<Row> rows;
  rows.reserve(input.event_count());
  // A listed name is a str: it matches only a text cell equal to it.
  std::map<std::string, std::string> text_resources;
  // The groupby keys: (activity, resource), and the group with a group
  // attribute; pandas sorts them, so values Python cannot order raise.
  std::vector<std::vector<python::Scalar>> pair_keys, group_keys;
  // The membership groupby [resource, group]: its groups in pandas' order
  // give each group's members (sorted by resource) and the groups' order.
  std::vector<python::Group> membership;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      Row row;
      const auto r = frame_row_scalar(frame, trace, event, resource_key);
      const auto a = frame_row_scalar(frame, trace, event, activity_key);
      const auto g = frame_row_scalar(frame, trace, event, group_key);
      if (r) {
        row.resource = &resources.add(*r);
        if (r->kind == python::Scalar::Kind::kText) text_resources.emplace(r->text, *row.resource);
      }
      if (a) row.activity = &activities.add(*a);
      if (g) row.group = &groups.add(*g);
      // Each level is sorted over every row, also the rows the other
      // level's null drops (round BB, observed black-box: an activity
      // column of dates and floats fails though only one row has a
      // resource).
      pair_keys.push_back({a.value_or(python::none()), r.value_or(python::none())});
      if (o.explicit_groups.empty()) group_keys.push_back({r.value_or(python::none()), g.value_or(python::none())});
      rows.push_back(row);
    }
  if (!pair_labels) {
    try {
      (void)python::groupby(pair_keys);
    } catch (const python::TypeError& error) {
      sort_error(error);
    }
  }
  if (!keys.contains(group_key)) throw std::invalid_argument("KeyError: " + python_repr(group_key));
  // C[r][a] with each resource's activities in first-seen order, and each
  // resource's rows (any activity).
  std::map<std::string, std::vector<std::pair<std::string, std::uint64_t>>> counts;
  std::map<std::string, std::uint64_t> resource_rows;
  std::vector<std::string> pair_resources;
  std::size_t skipped = 0;
  const auto count_pair = [&](const std::string& r, const std::string& a) -> std::uint64_t& {
    const auto [it, inserted] = counts.try_emplace(r);
    if (inserted) pair_resources.push_back(r);
    auto& pairs = it->second;
    auto pair = std::find_if(pairs.begin(), pairs.end(), [&](const auto& p) { return p.first == a; });
    if (pair == pairs.end()) pair = pairs.emplace(pairs.end(), a, 0);
    return pair->second;
  };
  // Group-attribute membership read from the row labels: (group identity,
  // resource identity) -> coverage count, groups in sorted label order.
  std::vector<std::pair<std::string, std::string>> label_members;
  std::map<std::string, std::map<std::string, std::uint64_t>> label_shares;
  if (pair_labels) {
    const auto by_label = [&](const std::string& first, const std::string& second, auto present) {
      std::map<std::string, std::uint64_t> labelled;
      labelled[first] += present(rows[0]) ? 1 : 0;
      labelled[second] += present(rows[1]) ? 1 : 0;
      return labelled;
    };
    for (const auto& [label, n] : by_label(activity_key, resource_key, [](const Row& row) { return row.group; })) {
      const auto [a, r] = label_characters(label);
      count_pair(resources.add(python::text(r)), activities.add(python::text(a))) = n;
    }
    for (const auto& row : rows)
      if (row.resource) ++resource_rows[*row.resource];
    if (o.explicit_groups.empty()) {
      if (keys.contains(resource_key) || !keys.contains(activity_key))
        throw std::invalid_argument("KeyError: " + python_repr(activity_key));
      for (const auto& [label, n] : by_label(resource_key, group_key, [](const Row& row) { return row.activity; })) {
        const auto [r, g] = label_characters(label);
        const auto& resource = resources.add(python::text(r));
        const auto& group = groups.add(python::text(g));
        label_members.emplace_back(group, resource);
        label_shares[group][resource] = n;
      }
    } else if (!keys.contains(resource_key)) {
      throw std::invalid_argument("KeyError: " + python_repr(resource_key));
    }
    if (resources.size() > o.max_resources)
      throw std::invalid_argument("local_diagnostics resource bound exceeded");
  } else {
    // A column named twice (the group as resource or activity, or with a
    // group attribute the resource as activity) is no single column to
    // pandas there: the counts' column is missing after the groupby (the
    // membership groupby [resource, group] sorts first; round BB, observed
    // black-box).
    if (group_key == resource_key || group_key == activity_key)
      throw std::invalid_argument("KeyError: " + python_repr(group_key));
    try {
      membership = python::groupby(group_keys);
    } catch (const python::TypeError& error) {
      sort_error(error);
    }
    if (o.explicit_groups.empty() && resource_key == activity_key)
      throw std::invalid_argument("KeyError: " + python_repr(resource_key));
    if (resources.size() > o.max_resources)
      throw std::invalid_argument("local_diagnostics resource bound exceeded");
    for (const auto& row : rows) {
      if (row.resource) ++resource_rows[*row.resource];
      if (!row.resource || !row.activity) {
        ++skipped;
        continue;
      }
      auto& n = count_pair(*row.resource, *row.activity);
      if (row.group) ++n;
    }
  }
  std::vector<std::string> activity_order;
  std::map<std::string, std::uint64_t> activity_totals;
  for (const auto& r : pair_resources)
    for (const auto& [a, n] : counts[r]) {
      if (!activity_totals.contains(a)) activity_order.push_back(a);
      activity_totals[a] += n;
    }

  // The groups: name, group identity (group-attribute mode) and members.
  struct Group {
    std::string name;
    std::string id;
    std::vector<std::string> members;
  };
  const bool group_attribute_mode = o.explicit_groups.empty();
  std::vector<Group> members;
  const auto add_member = [](std::vector<std::string>& list, const std::string& r) {
    if (std::find(list.begin(), list.end(), r) == list.end()) list.push_back(r);
  };
  if (group_attribute_mode && pair_labels) {
    std::map<std::string, std::size_t> position;
    for (const auto& [group, resource] : label_members) {
      const auto [it, inserted] = position.try_emplace(group, members.size());
      if (inserted) members.push_back({groups.name.at(group), group, {}});
      add_member(members[it->second].members, resource);
    }
  } else if (group_attribute_mode) {
    std::map<std::string, std::size_t> position;
    for (const auto& pair : membership) {
      const auto& resource = resources.add(pair.labels[0]);
      const auto& group = groups.add(pair.labels[1]);
      const auto [it, inserted] = position.try_emplace(group, members.size());
      if (inserted) members.push_back({groups.name.at(group), group, {}});
      add_member(members[it->second].members, resource);
    }
  } else {
    if (o.explicit_groups.size() > o.max_groups)
      throw std::invalid_argument("local_diagnostics group bound exceeded");
    for (const auto& [g, rs] : o.explicit_groups) {
      if (!nonblank(g)) throw std::invalid_argument("local_diagnostics group name is blank");
      Group group{g, {}, {}};
      for (const auto& r : rs) {
        const auto it = text_resources.find(r);
        if (it == text_resources.end()) throw std::invalid_argument("KeyError: " + python_repr(r));
        add_member(group.members, it->second);
      }
      if (group.members.size() > o.max_resources)
        throw std::invalid_argument("local_diagnostics resource bound exceeded");
      members.push_back(std::move(group));
    }
  }
  if (members.size() > o.max_groups) throw std::invalid_argument("local_diagnostics group bound exceeded");
  const auto count_of = [&](const std::string& r, const std::string& a) -> std::uint64_t {
    const auto it = counts.find(r);
    if (it == counts.end()) return 0;
    for (const auto& [activity, n] : it->second)
      if (activity == a) return n;
    return 0;
  };

  std::size_t leaves = 0;
  const auto add_leaf = [&]() {
    if (leaves >= o.max_rows) {
      result.diagnostics.output_truncated = true;
      return false;
    }
    ++leaves;
    return true;
  };
  for (const auto& group : members) {
    // Every group is a key of each dict, empty when nothing is counted (no
    // activity, or no member).
    (void)result.group_relative_focus[group.name];
    (void)result.group_relative_stake[group.name];
    (void)result.group_coverage[group.name];
    std::map<std::string, std::uint64_t> group_counts;
    std::uint64_t group_total = 0;
    for (const auto& a : activity_order) {
      std::uint64_t n = 0;
      for (const auto& r : group.members) n += count_of(r, a);
      group_counts[a] = n;
      group_total += n;
    }
    for (const auto& a : activity_order) {
      if (activity_totals[a] == 0) zero_division();
      if (add_leaf())
        result.group_relative_focus[group.name][activities.name.at(a)] =
            static_cast<double>(group_counts[a]) / static_cast<double>(activity_totals[a]);
    }
    for (const auto& a : activity_order) {
      if (group_total == 0) zero_division();
      if (add_leaf())
        result.group_relative_stake[group.name][activities.name.at(a)] =
            static_cast<double>(group_counts[a]) / static_cast<double>(group_total);
    }
    std::map<std::string, std::uint64_t> share;
    std::uint64_t share_total = 0;
    if (group_attribute_mode && pair_labels) {
      for (const auto& [r, n] : label_shares[group.id]) {
        share[r] = n;
        share_total += n;
      }
    } else if (group_attribute_mode) {
      for (const auto& row : rows) {
        if (!row.resource || !row.activity || !row.group || *row.group != group.id) continue;
        ++share[*row.resource];
        ++share_total;
      }
    } else {
      for (const auto& r : group.members) {
        share[r] = resource_rows[r];
        share_total += resource_rows[r];
      }
    }
    for (const auto& r : group.members) {
      if (share_total == 0) zero_division();
      if (add_leaf())
        result.group_coverage[group.name][resources.name.at(r)] =
            static_cast<double>(share[r]) / static_cast<double>(share_total);
    }
  }
  for (const auto& group : members) {
    auto& by_resource = result.group_member_contribution[group.name];
    for (const auto& r : group.members) {
      const auto it = counts.find(r);
      if (it == counts.end())
        throw std::invalid_argument(
            "KeyError: " + resources.repr(r, frame_column_type(frame, resource_key) == DataFrameColumnType::kObject));
      auto& out = by_resource[resources.name.at(r)];
      for (const auto& [a, n] : it->second)
        if (add_leaf()) out[activities.name.at(a)] = n;
    }
  }
  result.diagnostics.group_count = members.size();
  result.diagnostics.resource_count = resources.size();
  result.diagnostics.skipped_events = skipped;
  result.diagnostics.row_count = leaves;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated
                                   ? "row safety bound applied"
                                   : (leaves ? "ok" : "no matching data");
  return result;
}

}  // namespace

LocalDiagnosticsResult discover_local_diagnostics(const EventLog& log,
                                                  const LocalDiagnosticsOptions& o) {
  if (o.max_groups == 0 || o.max_groups > kHardGroups || o.max_resources == 0 ||
      o.max_resources > kHardResources || o.max_rows == 0 || o.max_rows > kHardRows)
    throw std::invalid_argument("local_diagnostics safety bound exceeded");
  if (log.dataframe_columns) return frame_local_diagnostics(log, o);
  LocalDiagnosticsResult result;
  result.diagnostics.trace_count = log.trace_count();
  result.diagnostics.event_count = log.event_count();

  std::map<std::string, std::set<std::string>> members;
  std::map<std::string, std::map<std::string, std::uint64_t>> tagged;
  const bool group_attribute_mode = o.explicit_groups.empty();
  std::set<std::string> all_resources;
  if (!o.explicit_groups.empty()) {
    if (o.explicit_groups.size() > o.max_groups) throw std::invalid_argument("local_diagnostics group bound exceeded");
    for (const auto& [g, rs] : o.explicit_groups) {
      if (!nonblank(g)) throw std::invalid_argument("local_diagnostics group name is blank");
      auto& set = members[g];
      for (const auto& r : rs) if (nonblank(r)) set.insert(r);
      if (set.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");
      all_resources.insert(set.begin(), set.end());
    }
    if (all_resources.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");
  } else {
    // Group-attribute mode, as the reference apply_from_group_attribute: a
    // group is the set of resources seen with that group value, and every
    // event of a member then counts for each of its groups, whatever group
    // value (if any) that particular event carries.
    // Coverage alone stays on the tagged events: the share of a group's
    // tagged events that each member performed.
    for (const auto& tr : log.traces) for (const auto& e : tr.events) {
      const std::string resource = value_for(e, o.resource_attribute, true);
      if (!nonblank(resource)) continue;
      const auto* gv = e.attribute(o.group_attribute);
      if (!gv || !nonblank(*gv)) continue;
      auto& set = members[*gv];
      set.insert(resource);
      all_resources.insert(resource);
      if (nonblank(value_for(e, o.activity_attribute, false))) ++tagged[*gv][resource];
      if (members.size() > o.max_groups) throw std::invalid_argument("local_diagnostics group bound exceeded");
      if (set.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");
    }
  }

  // Raw counts by group, resource and activity: every event of a member
  // resource counts for each group that contains it.
  using Triple = std::map<std::string, std::map<std::string, std::map<std::string, std::uint64_t>>>;
  Triple counts;
  std::map<std::string, std::uint64_t> activity_totals;
  std::size_t skipped = 0;
  for (const auto& tr : log.traces) for (const auto& e : tr.events) {
    std::string activity = value_for(e, o.activity_attribute, false);
    std::string resource = value_for(e, o.resource_attribute, true);
    if (!nonblank(activity) || !nonblank(resource)) { ++skipped; continue; }
    all_resources.insert(resource);
    ++activity_totals[activity];
    if (all_resources.size() > o.max_resources)
      throw std::invalid_argument("local_diagnostics resource bound exceeded");
    std::vector<std::string> groups;
    for (const auto& [g, rs] : members) if (rs.contains(resource)) groups.push_back(g);
    if (groups.empty()) { ++skipped; continue; }
    for (const auto& g : groups) {
      ++counts[g][resource][activity];
    }
  }
  if (all_resources.size() > o.max_resources) throw std::invalid_argument("local_diagnostics resource bound exceeded");

  std::map<std::string, std::uint64_t> group_totals;
  std::map<std::string, std::map<std::string, std::uint64_t>> group_activity_totals;
  for (const auto& [g, rs] : counts) for (const auto& [r, acts] : rs) for (const auto& [a, n] : acts) group_totals[g] += n;
  for (const auto& [g, rs] : counts) for (const auto& [r, acts] : rs) for (const auto& [a, n] : acts) group_activity_totals[g][a] += n;
  result.diagnostics.group_count = members.size();
  result.diagnostics.resource_count = all_resources.size();
  result.diagnostics.skipped_events = skipped;
  std::size_t rows = 0;
  const auto add_metric_row = [&]() {
    if (rows >= o.max_rows) {
      result.diagnostics.output_truncated = true;
      return false;
    }
    ++rows;
    return true;
  };
  for (const auto& [g, rs] : members) {
    for (const auto& [a, total] : activity_totals) {
      const auto group_count = group_activity_totals[g][a];
      if (add_metric_row()) {
        result.group_relative_focus[g][a] = total
            ? static_cast<double>(group_count) / total : 0.0;
      }
      if (add_metric_row()) {
        result.group_relative_stake[g][a] = group_totals[g]
            ? static_cast<double>(group_count) / group_totals[g] : 0.0;
      }
    }
    std::uint64_t coverage_total = group_totals[g];
    if (group_attribute_mode) {
      coverage_total = 0;
      for (const auto& [r, n] : tagged[g]) { (void)r; coverage_total += n; }
    }
    for (const auto& r : rs) {
      std::uint64_t resource_total = 0;
      if (group_attribute_mode) {
        if (const auto it = tagged[g].find(r); it != tagged[g].end()) resource_total = it->second;
      } else if (const auto group_it = counts.find(g); group_it != counts.end()) {
        if (const auto resource_it = group_it->second.find(r); resource_it != group_it->second.end()) {
          for (const auto& [a, n] : resource_it->second) resource_total += n;
        }
      }
      if (add_metric_row()) {
        result.group_coverage[g][r] = coverage_total
            ? static_cast<double>(resource_total) / coverage_total : 0.0;
      }
    }
  }
  for (const auto& [g, rs] : counts) {
    for (const auto& [r, acts] : rs) {
      for (const auto& [a, n] : acts) {
        if (add_metric_row()) result.group_member_contribution[g][r][a] = n;
      }
    }
  }
  result.diagnostics.row_count = rows;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.output_truncated ? "row safety bound applied" : (rows ? "ok" : "no matching data");
  return result;
}

LocalDiagnosticsResult apply_from_group_attribute(const EventLog& log,
                                                  const LocalDiagnosticsOptions& options) {
  auto mode = options;
  mode.explicit_groups.clear();
  return discover_local_diagnostics(log, mode);
}

LocalDiagnosticsResult apply_from_clustering_or_roles(
    const EventLog& log,
    const std::map<std::string, std::vector<std::string>>& groups,
    LocalDiagnosticsOptions options) {
  options.explicit_groups = groups;
  return discover_local_diagnostics(log, options);
}

std::string local_diagnostics_json(const LocalDiagnosticsResult& r) {
  const auto finite_ratios = [](const auto& values) {
    for (const auto& [outer, inner] : values) {
      (void)outer;
      for (const auto& [key, value] : inner) {
        (void)key;
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) return false;
      }
    }
    return true;
  };
  if (!finite_ratios(r.group_relative_focus) ||
      !finite_ratios(r.group_relative_stake) ||
      !finite_ratios(r.group_coverage))
    throw std::invalid_argument("local diagnostics ratio is invalid");
  std::set<std::string> groups;
  std::set<std::string> resources;
  std::size_t rows = 0;
  for (const auto& [group, values] : r.group_relative_focus) {
    rows += values.size(); groups.insert(group);
  }
  for (const auto& [group, values] : r.group_relative_stake) {
    rows += values.size(); groups.insert(group);
  }
  for (const auto& [group, values] : r.group_coverage) {
    rows += values.size();
    groups.insert(group);
    for (const auto& [resource, value] : values) {
      (void)value; resources.insert(resource);
    }
  }
  for (const auto& [group, by_resource] : r.group_member_contribution) {
    groups.insert(group);
    for (const auto& [resource, activities] : by_resource) {
      rows += activities.size(); resources.insert(resource);
    }
  }
  // Membership totals include groups with no scalar output, and truncation
  // may omit whole groups. Serialized leaves still exactly match row_count.
  if (r.diagnostics.group_count < groups.size() ||
      r.diagnostics.resource_count < resources.size() ||
      r.diagnostics.row_count != rows ||
      r.diagnostics.skipped_events > r.diagnostics.event_count)
    throw std::invalid_argument("local diagnostics counts are inconsistent");
  std::ostringstream out;
  out << "{\"family\":\"LOCAL_DIAGNOSTICS\",\"group_relative_focus\":";
  emit_double_map(out, r.group_relative_focus);
  out << ",\"group_relative_stake\":"; emit_double_map(out, r.group_relative_stake);
  out << ",\"group_coverage\":"; emit_double_map(out, r.group_coverage);
  out << ",\"group_member_contribution\":{";
  bool fg = true;
  for (const auto& [g, rs] : r.group_member_contribution) {
    if (!fg) out << ','; fg = false; quote(out, g); out << ":{"; bool fr = true;
    for (const auto& [res, acts] : rs) { if (!fr) out << ','; fr = false; quote(out, res); out << ":{"; bool fa = true; for (const auto& [a,n] : acts) { if (!fa) out << ','; fa = false; quote(out,a); out << ':' << n; } out << '}'; }
    out << '}';
  }
  out << "},\"diagnostics\":{";
  out << "\"computed\":" << (r.diagnostics.computed ? "true" : "false")
      << ",\"output_truncated\":" << (r.diagnostics.output_truncated ? "true" : "false")
      << ",\"trace_count\":" << r.diagnostics.trace_count << ",\"event_count\":" << r.diagnostics.event_count
      << ",\"group_count\":" << r.diagnostics.group_count << ",\"resource_count\":" << r.diagnostics.resource_count
      << ",\"row_count\":" << r.diagnostics.row_count << ",\"skipped_events\":" << r.diagnostics.skipped_events << ",\"message\":";
  quote(out, r.diagnostics.message); out << "}}";
  return out.str();
}
}  // namespace minera
