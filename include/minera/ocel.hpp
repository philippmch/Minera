#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace minera {

using OcelValue = std::variant<std::nullptr_t, bool, std::int64_t, double,
                               std::string>;

struct OcelAttribute {
  std::string key;
  OcelValue value;
};

struct OcelEvent {
  std::string id;
  std::string activity;
  std::string timestamp;
  std::vector<OcelAttribute> attributes;
};

struct OcelObject {
  std::string id;
  std::string type;
  std::vector<OcelAttribute> attributes;
};

struct OcelRelation {
  std::string event_id;
  std::string object_id;
  std::string qualifier;
};

struct OcelO2oRelation {
  std::string source_id;
  std::string target_id;
  std::string qualifier;
};

struct OcelObjectChange {
  std::string object_id;
  std::string object_type;
  std::string timestamp;
  std::string field;
  OcelValue value;
};

struct Ocel {
  std::vector<OcelEvent> events;
  std::vector<OcelObject> objects;
  std::vector<OcelRelation> relations;
  std::vector<OcelO2oRelation> o2o;
  std::vector<OcelObjectChange> object_changes;
  std::map<std::string, OcelValue> globals;
};

using OCEL = Ocel;

struct OcelJsonLimits {
  std::size_t max_bytes = 16u * 1024u * 1024u;
  std::size_t max_depth = 64;
  std::size_t max_string_bytes = 1u * 1024u * 1024u;
  std::size_t max_nodes = 1u * 1024u * 1024u;
};

enum class OcelJsonFormat { kClassic, kStandard };

Ocel parse_ocel_json(std::string_view json,
                     const OcelJsonLimits& limits = {});
std::string serialize_ocel_json(const Ocel& log,
                                OcelJsonFormat format = OcelJsonFormat::kClassic);

struct EtotEdge {
  std::string activity;
  std::string object_type;
  std::size_t frequency = 0;
};

struct EtotGraph {
  std::vector<std::string> activities;
  std::vector<std::string> object_types;
  std::vector<EtotEdge> edges;
};

EtotGraph discover_etot_classic(const Ocel& log);

struct OtgEdge {
  std::string source_type;
  std::string relation;
  std::string target_type;
  std::size_t frequency = 0;
};

struct OtgGraph {
  std::vector<std::string> object_types;
  std::vector<OtgEdge> edges;
};

OtgGraph discover_otg_classic(const Ocel& log);

[[nodiscard]] std::string etot_json(const EtotGraph& graph);
[[nodiscard]] std::string otg_json(const OtgGraph& graph);

struct OcdfgActivityMetric {
  std::vector<std::string> events;
  std::vector<std::string> unique_objects;
  std::vector<std::pair<std::string, std::string>> event_objects;
};

struct OcdfgEdgeMetric {
  std::vector<std::pair<std::string, std::string>> event_couples;
  std::vector<std::string> unique_objects;
  std::vector<std::tuple<std::string, std::string, std::string>> event_objects;
  std::vector<double> performance;
};

struct OcdfgGraph {
  std::vector<std::string> activities;
  std::vector<std::string> object_types;
  std::map<std::string, OcdfgActivityMetric> activities_indep;
  std::map<std::string, std::map<std::string, OcdfgActivityMetric>> activities_ot;
  std::map<std::string, std::map<std::string, OcdfgActivityMetric>> start_activities;
  std::map<std::string, std::map<std::string, OcdfgActivityMetric>> end_activities;
  std::map<std::string, std::map<std::pair<std::string, std::string>, OcdfgEdgeMetric>> edges;
  bool performance_computed = true;
};

OcdfgGraph discover_ocdfg_classic(const Ocel& log,
                                  bool compute_edges_performance = true);
std::string serialize_ocdfg_json(const OcdfgGraph& graph);

}  // namespace minera
