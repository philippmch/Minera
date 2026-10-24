#include "../log_timestamp_original_call_parser.hpp"
#include <bit>
#include <iostream>

namespace {
using namespace minera;
namespace original = minera::log_timestamp_original_call_detail;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string descriptor(std::string args, std::string keywords = "[]", std::string selected = "null") {
  return "{\"schema\":\"minera-log-timestamp-original-call-v1\",\"args\":" + args + ",\"keyword_entries\":" + keywords + ",\"source_trace_index\":" + selected + '}';
}
EventAttribute value(const std::string& raw) { const auto node = original::shared::Reader(raw).read(); original::validate(raw, node); return original::packed(raw, node); }
void controls() {
  const auto tuple = value(R"({"type":"tuple","values":[{"type":"float","hex":"-0x0.0p+0"},{"type":"list","values":[{"type":"int","value":9007199254740993}]}]})");
  require(tuple.container == AttributeContainerKind::kTuple && tuple.children[0].key == "0" && tuple.children[1].children[0].key == "0", "native indexed keys changed");
  require(tuple.children[0].value == std::to_string(-0.0) && std::bit_cast<std::uint64_t>(std::get<double>(tuple.children[0].typed_value)) == std::bit_cast<std::uint64_t>(-0.0), "signed zero changed");
  require(std::get<std::int64_t>(tuple.children[1].children[0].typed_value) == INT64_C(9007199254740993), "exact int64 lost");
  require(std::get<std::uint64_t>(value(R"({"type":"int","value":18446744073709551615})").typed_value) == UINT64_MAX, "uint64 max lost");
  require(std::holds_alternative<std::monostate>(value(R"({"type":"int","value":-999999999999999999999999999})").typed_value), "lexical big integer coerced");
  const auto date = value(R"({"type":"datetime","iso":"2026-06-01T00:00:00","fold":1,"offset_seconds":null})");
  const auto& temporal = std::get<NativeDate>(date.typed_value);
  require(temporal.resolution == NativeDateResolution::kMicrosecond && !temporal.timezone, "NativeDate defaults changed");
  const auto nat = value(R"({"type":"nat"})");
  require(nat.is_null && std::get<NativeDate>(nat.typed_value).resolution == NativeDateResolution::kNanosecond, "NaT changed");
  require(is_native_na_attribute(value(R"({"type":"native_na"})")), "native NA lost");
  require(decode_bytes_attribute(value(R"({"type":"bytes","hex":"ff000a"})")) == std::string("\xff\0\n", 3), "bytes changed");
  require(value(R"({"type":"uuid","value":"12345678-1234-5678-1234-567812345678"})").kind == AttributeKind::kId, "UUID kind lost");
  const auto duplicate_pairs = value(R"({"type":"dict","items":[[{"type":"str","value":"same"},{"type":"int","value":1}],[{"type":"str","value":"same"},{"type":"int","value":2}]]})");
  require(duplicate_pairs.children.size() == 2 && duplicate_pairs.children[1].value == "2", "ordered pair entries collapsed");
  for (const auto& kind : {"set", "frozenset"}) {
    const auto item = value("{\"type\":\"" + std::string(kind) + "\",\"values\":[{\"type\":\"str\",\"value\":\"A\"}]}");
    require(item.children[0].key == "0", "set indexed child lost");
  }
  for (const auto& hex : {"nan", "inf", "-inf", "0x1.5555555555555p-2"}) {
    const auto item = value("{\"type\":\"float\",\"hex\":\"" + std::string(hex) + "\"}");
    require(item.value == std::to_string(std::get<double>(item.typed_value)), "float display differs from actual native double");
  }
  const std::string bounds = R"([{"type":"datetime","iso":"2026-06-01T00:00:00Z","fold":0,"offset_seconds":0},{"type":"datetime","iso":"2026-06-01T00:00:02Z","fold":0,"offset_seconds":0}])";
  const std::string parameters = R"([{"key":"parameters","value":{"type":"dict","items":[[{"type":"str","value":"pm4py:param:timestamp_key"},{"type":"str","value":"when"}],[{"type":"enum","module":"pm4py.algo.filtering.log.timestamp.timestamp_filter","name":"TIMESTAMP_KEY","value":{"type":"str","value":"pm4py:param:timestamp_key"}},{"type":"str","value":"enum_when"}],[{"type":"str","value":"positive"},{"type":"bool","value":false}],[{"type":"str","value":"ignored"},{"type":"enum","module":"opaque","name":"NEVER_NATIVE","value":{"type":"str","value":"raw"}}]]}}])";
  auto call = original::parse(descriptor(bounds, parameters), "apply_events");
  require(call.options && call.options->timestamp_key.value == "when" && call.options->enum_timestamp_key->value == "enum_when" && !std::get<bool>(call.options->positive.typed_value), "actual Options assignments lost");
  require(call.projected.size() == 3 && call.unprojected_parameters == std::vector<std::size_t>{3}, "ignored/projected metadata wrong");
  bool unbound_rejected = false; try { (void)original::projection_json(call, "apply_events"); } catch (const std::logic_error&) { unbound_rejected = true; }
  require(unbound_rejected, "projection fabricated before actual binding");
  EventLog source; source.traces.emplace_back(); original::bind(call, source);
  const auto before = original::controls_json(call, source);
  call.args[0].key = "actual-stored-key";
  call.options->timestamp_key = EventAttribute{"actual-options-key", "changed"};
  source.traces[0].properties = {{"actual-trace-property", "changed"}};
  const auto after = original::controls_json(call, source);
  require(before != after && after.find("actual-stored-key") != std::string::npos && after.find("actual-options-key") != std::string::npos && after.find("actual-trace-property") != std::string::npos, "controls came from stale descriptor/mirror");
  const auto complete = original::shared::Reader(after).read();
  require(complete.fields.size() == 4 && original::at(complete, "options").fields.size() == 3, "controls exact schema changed");
  const auto& arg = original::at(complete, "arguments").items[0];
  const auto table = parse_embedding_workbench_table_json(original::string(original::at(arg, "value_json")));
  require(table.columns == std::vector<std::string>{"actual-stored-key"} && table.rows[0][0]->key == "actual-stored-key", "full cell key was renamed");
  auto raw = original::parse(descriptor(bounds, R"([{"key":"parameters","value":{"type":"native_na"}}])"), "apply_events");
  require(raw.options && raw.parameter_state == "non_dictionary" && raw.options->timestamp_key.value == "time:timestamp" && std::holds_alternative<std::monostate>(raw.options->positive.typed_value), "non-dictionary parameters falsely constructed as native operand");
  auto stub = original::parse(descriptor("[]", R"([{"key":"parameters","value":{"type":"native_na"}}])"), "apply");
  original::bind(stub, source);
  const auto projection = original::projection_json(stub, "apply");
  require(!stub.options && projection.find("implicit_default_not_inspected") != std::string::npos && stub.unprojected_keywords == std::vector<std::size_t>{0}, "stub default syntax misreported");
  const std::string empty_dict = R"({"type":"dict","items":[]})";
  const std::string trace = "{\"attributes\":" + empty_dict + ",\"properties\":" + empty_dict + ",\"events\":[]}";
  const std::string log = "{\"type\":\"log\",\"snapshot\":{\"attributes\":" + empty_dict + ",\"extensions\":" + empty_dict + ",\"classifiers\":" + empty_dict + ",\"omni_present\":" + empty_dict + ",\"properties\":" + empty_dict + ",\"traces\":[" + trace + "]}}";
  const auto grammar18 = original::parse(descriptor("[]", "[{\"key\":\"log_metadata\",\"value\":" + log + "},{\"key\":\"trace_metadata\",\"value\":{\"type\":\"trace\",\"snapshot\":" + trace + ",\"source_trace_index\":null}}]"), "apply");
  require(grammar18.unprojected_keywords.size() == 2, "opaque log/trace grammar lost");
  auto predicate = original::parse(descriptor(R"([{"type":"nat"},{"type":"nat"},{"type":"str","value":"time:timestamp"}])", "[]", "0"), "is_contained");
  original::bind(predicate, source);
  source.traces[0].properties[0].value = "actual-second-state";
  require(original::controls_json(predicate, source).find("actual-second-state") != std::string::npos, "selected trace controls use a mirrored snapshot");
  for (const auto& bad : std::vector<std::string>{
      descriptor("[]", "[]", "0"), descriptor("[]", R"([{"key":"x","value":{"type":"int","value":1}},{"key":"x","value":{"type":"int","value":2}}])"),
      R"({"schema":"minera-log-timestamp-original-call-v1","args":[],"\u0061rgs":[],"keyword_entries":[],"source_trace_index":null})",
      descriptor("[]", R"([{"key":"x","value":{"type":"float","hex":"0x1p+0garbage"}}])"),
      descriptor("[]", R"([{"key":"x","value":{"type":"int","value":1.0}}])"),
      descriptor("[]", R"([{"key":"x","value":{"type":"datetime","iso":"x","fold":0,"offset_seconds":1e9999}}])"),
      descriptor("[]", R"([{"key":"x","value":{"type":"bytes","hex":"FF"}}])"),
      descriptor("[]", R"([{"key":"x","value":{"type":"trace","snapshot":{},"source_trace_index":null}}])"),
      descriptor("[]", "[]") + " trailing", std::string("\xff", 1) + descriptor("[]")}) {
    bool rejected = false; try { (void)original::parse(bad, "apply"); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "malformed descriptor accepted");
  }
  const auto bom = original::parse(std::string("\xef\xbb\xbf", 3) + "\r\n" + descriptor(bounds), "apply_events");
  require(bom.args[0].value == "2026-06-01T00:00:00Z", "BOM view changed operand slices");
}
}
int main() { try { controls(); std::cout << "LOG timestamp Packed18/native controls checks passed\n"; } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } return 0; }
