#include "minera/log_to_features.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace minera;

static EventLog fixture() {
  EventLog log;
  Trace t; t.case_id = "c1";
  t.attributes.push_back({"segment", "gold", AttributeKind::kString});
  Event a{"c1", "A", "2024-01-01T00:00:00Z", "r1", 0};
  a.attributes.push_back({"amount", "2.5", AttributeKind::kFloat});
  a.attributes.push_back({"kind", "x", AttributeKind::kString});
  Event b{"c1", "B", "2024-01-01T01:00:00Z", "r1", 1};
  b.attributes.push_back({"amount", "3", AttributeKind::kFloat});
  t.events = {a, b}; log.traces.push_back(t); return log;
}

int main() {
  const auto log = fixture();
  LogToFeaturesOptions e; e.event_numeric_attributes = {"amount"}; e.event_string_attributes = {"kind"}; e.case_string_attributes = {"segment"};
  const auto er = log_to_features(log, e);
  assert(er.event_rows.size() == 1 && er.event_rows[0].size() == 2);
  assert(!er.feature_names.empty());
  e.variant = LogToFeaturesVariant::TRACE_BASED;
  const auto tr = log_to_features(log, e);
  assert(tr.trace_rows.size() == 1 && tr.trace_rows[0].size() == tr.feature_names.size());
  e.variant = LogToFeaturesVariant::TEMPORAL; e.window_seconds = 3600;
  const auto tm = log_to_features(log, e);
  assert(tm.temporal_rows.size() == 2 && tm.temporal_rows[0].values.size() == tm.feature_names.size());
  bool threw = false; try { e.window_seconds = 0; (void)log_to_features(log, e); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog bad = log; bad.traces[0].events[0].timestamp = "nonsense"; e.window_seconds = 3600; threw = false;
  try { (void)log_to_features(bad, e); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
