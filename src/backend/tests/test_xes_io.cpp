#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
#include "minera/log_conversion.hpp"
#include "../gzip_codec.hpp"

namespace {

bool throws(const auto& operation) {
  try {
    operation();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

std::string decode_base64(std::string_view encoded) {
  static constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  unsigned accumulator = 0;
  unsigned bits = 0;
  for (const char character : encoded) {
    if (character == '=') break;
    const auto position = alphabet.find(character);
    if (position == std::string_view::npos)
      throw std::runtime_error("invalid base64 fixture");
    accumulator = (accumulator << 6U) | static_cast<unsigned>(position);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      output.push_back(static_cast<char>((accumulator >> bits) & 0xffU));
    }
  }
  return output;
}

minera::EventLog sample_log() {
  minera::EventLog log;
  log.attributes.push_back({"source", "gzip-test"});
  minera::Trace trace;
  trace.case_id = "case-1";
  trace.attributes.push_back({"region", "west"});
  trace.events.push_back(
      {"case-1", "Review & approve", "2026-01-01T00:00:00Z", "alice", 0});
  trace.events.back().attributes.push_back(
      {"cost", "12.5", minera::AttributeKind::kFloat});
  log.traces.push_back(std::move(trace));
  return log;
}

const minera::EventAttribute* find_attribute(
    const std::vector<minera::EventAttribute>& attributes,
    std::string_view key) {
  for (const auto& attribute : attributes) {
    if (attribute.key == key) return &attribute;
  }
  return nullptr;
}

std::size_t occurrences(std::string_view text, std::string_view needle) {
  std::size_t count = 0;
  for (std::size_t position = 0;
       (position = text.find(needle, position)) != std::string_view::npos;
       position += needle.size()) {
    ++count;
  }
  return count;
}

void variants_and_gzip_round_trip() {
  const auto log = sample_log();
  for (const auto variant : {minera::XesExportVariant::LINE_BY_LINE,
                             minera::XesExportVariant::ETREE}) {
    minera::XesExportOptions options;
    options.variant = variant;
    options.compress = true;
    options.name = "compressed log";
    const auto bytes = minera::serialize_xes(log, options);
    assert(bytes.size() > 18);
    assert(static_cast<unsigned char>(bytes[0]) == 0x1f);
    assert(static_cast<unsigned char>(bytes[1]) == 0x8b);

    std::istringstream input(bytes);
    const auto restored = minera::import_xes_event_log(input);
    assert(restored.trace_count() == 1);
    assert(restored.event_count() == 1);
    assert(restored.traces[0].events[0].activity == "Review & approve");
    assert(restored.traces[0].events[0].resource == "alice");
    assert(*restored.traces[0].attribute("region") == "west");
    assert(*restored.traces[0].events[0].attribute("cost") == "12.5");
    assert(*restored.attribute("source") == "gzip-test");
    assert(minera::parse_xes_export_variant(
               minera::xes_export_variant_name(variant)) == variant);
  }
  assert(throws([] { (void)minera::parse_xes_export_variant("unknown"); }));
}

void imports_external_dynamic_huffman_gzip() {
  // Independently compressed XML containing one trace and eighty events.
  const auto bytes = decode_base64(
      "H4sIAAAAAAAACp3YS2rCUBSA4a1INhDP25YY6FJCuIjUxmKj4O5FcNKZ/PPzzz7u4wyn82Ec1ss0t3H4Wy/H5bD5bvd9N5+Xuf2un8v007rNbTpd276bu34c2q0t6xvDX/Kc7l/jb1eKKkOVoypQlagqVO1Q9YEq2bKM6RDGQ5gPYUCECRFGRJgRYUiEKVGmROEZwpQoU6JMiTIlypQoU6JMiTIlxpQYU2LwqmFKjCkxpsSYEmNKjCkxpsSZEmdKnClx+CJhSpwpcabEmRJnSpwpCaYkmJJgSoIpCfhwZUqCKQmmJJiSYEqSKUmmJJmSZEqSKUn4v2FKkilJpiSZkmJKiikppqSYkmJKiikp+A1mSoopKaZk909J/1qq9M8NywMkmd3BZxEAAA==");
  std::istringstream input(bytes);
  const auto log = minera::import_xes_event_log(input);
  assert(log.trace_count() == 1);
  assert(log.event_count() == 80);
  assert(log.traces[0].events.front().activity == "A1");
  assert(log.traces[0].events.back().activity == "A80");
}

void imports_fixed_huffman_and_concatenated_gzip() {
  const auto fixed = decode_base64(
      "H4sIAAAAAAACCrPJyU+3sykpSkxOtbMpLinKzEtXyE6ttFVKzs9LTi0oscpLzE1VUihLzClNtVVKy6xITVHSt7NJLUvNKyFCgyNIsT5UtT7UGn2QnQD3b6NmeQAAAA==");
  std::istringstream fixed_input(fixed);
  const auto fixed_log = minera::import_xes_event_log(fixed_input);
  assert(fixed_log.trace_count() == 1);
  assert(fixed_log.event_count() == 1);
  assert(fixed_log.traces[0].case_id == "fixed");

  const std::string prefix =
      "<log><trace><string key=\"concept:name\" value=\"joined\"/>";
  const std::string suffix =
      "<event><string key=\"concept:name\" value=\"B\"/></event></trace></log>";
  const auto joined = minera::detail::gzip_compress(prefix) +
                      minera::detail::gzip_compress(suffix);
  std::istringstream joined_input(joined);
  const auto joined_log = minera::import_xes_event_log(joined_input);
  assert(joined_log.trace_count() == 1);
  assert(joined_log.event_count() == 1);
  assert(joined_log.traces[0].case_id == "joined");
  assert(joined_log.traces[0].events[0].activity == "B");
}

void rejects_corruption_and_expansion_beyond_limit() {
  minera::XesExportOptions options;
  options.compress = true;
  auto bytes = minera::serialize_xes(sample_log(), options);
  bytes[bytes.size() - 8] = static_cast<char>(bytes[bytes.size() - 8] ^ 1);
  assert(throws([&] {
    std::istringstream input(bytes);
    (void)minera::import_xes_event_log(input);
  }));

  const auto valid = minera::serialize_xes(sample_log(), options);
  minera::XesImportOptions bounded;
  bounded.max_decompressed_bytes = 16;
  assert(throws([&] {
    std::istringstream input(valid);
    (void)minera::import_xes_event_log(input, bounded);
  }));
}

void file_export_uses_gzip_suffix_and_imports_it() {
  const auto requested = std::filesystem::temp_directory_path() /
                         "minera_xes_io_test.xes";
  const auto compressed = std::filesystem::path(requested.string() + ".gz");
  std::error_code ignored;
  std::filesystem::remove(requested, ignored);
  std::filesystem::remove(compressed, ignored);

  minera::XesExportOptions options;
  options.compress = true;
  const auto actual =
      minera::export_xes_event_log(requested, sample_log(), options);
  assert(actual == compressed);
  assert(std::filesystem::exists(compressed));
  assert(minera::import_xes_event_log(compressed).event_count() == 1);
  std::filesystem::remove(compressed, ignored);
}

void metadata_import_export_import_round_trip() {
  // All declarations are intentionally interleaved in the XML document.  The
  // native metadata vectors retain order within each declaration family.
  const std::string xml =
      "<log xes.version=\"1849-2016\" xes.features=\"nested-attributes\" "
      "xmlns=\"http://www.xes-standard.org/\">"
      "<extension name=\"Concept\" prefix=\"concept\" "
      "uri=\"http://www.xes-standard.org/concept.xesext\"/>"
      "<extension name=\"Time\" prefix=\"time\" "
      "uri=\"http://www.xes-standard.org/time.xesext\"/>"
      "<extension name=\"Custom &amp; Data\" prefix=\"custom\" "
      "uri=\"https://example.test/custom.xesext\"/>"
      "<classifier name=\"Activity\" keys=\"concept:name\"/>"
      "<classifier name=\"Case and life\" "
      "keys=\"'case id' 'life\\'cycle' activity\"/>"
      "<global scope=\"trace\">"
      "<string key=\"trace label\" value=\"\"/>"
      "<int key=\"trace count\" value=\"2\"/>"
      "<string key=\"concept:name\" value=\"\"/>"
      "</global>"
      "<global scope=\"event\">"
      "<boolean key=\"approved\" value=\"true\"/>"
      "<date key=\"event day\" value=\"2026-01-01T00:00:00.000Z\"/>"
      "<string key=\"concept:name\" value=\"\"/>"
      "</global>"
      "<string key=\"source\" value=\"fixture\"/>"
      "<int key=\"version\" value=\"7\"/>"
      "<trace>"
      "<string key=\"concept:name\" value=\"case-1\"/>"
      "<string key=\"region\" value=\"west\"/>"
      "<event>"
      "<string key=\"concept:name\" value=\"Review\"/>"
      "<string key=\"empty\" value=\"\"/>"
      "</event>"
      "</trace>"
      "</log>";

  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input);
  assert(log.metadata.extensions.size() == 3);
  assert(log.metadata.extensions[2].name == "Custom & Data");
  assert(log.metadata.classifiers.size() == 2);
  assert((log.metadata.classifiers[1].keys ==
          std::vector<std::string>{"case id", "life'cycle", "activity"}));
  assert(log.metadata.trace_globals.size() == 3);
  assert(log.metadata.trace_globals[0].key == "trace label");
  assert(log.metadata.trace_globals[0].value.empty());
  assert(log.metadata.event_globals.size() == 3);
  assert(log.metadata.event_globals[0].key == "approved");
  assert(log.metadata.properties.empty());
  assert(log.attributes.size() == 2);
  assert(log.attributes[0].key == "source");
  assert(log.attributes[1].key == "version");
  assert(log.traces[0].attributes.size() == 1);
  assert(log.traces[0].attributes[0].key == "region");
  assert(log.traces[0].events[0].attributes.size() == 1);
  assert(log.traces[0].events[0].attributes[0].key == "empty");

  for (const auto variant : {minera::XesImportVariant::ITERPARSE,
                             minera::XesImportVariant::LINE_BY_LINE,
                             minera::XesImportVariant::ITERPARSE_MEM_COMPRESSED,
                             minera::XesImportVariant::ITERPARSE_20,
                             minera::XesImportVariant::CHUNK_REGEX,
                             minera::XesImportVariant::RUSTXES}) {
    minera::XesImportOptions options;
    options.variant = variant;
    std::istringstream variant_input(xml);
    const auto variant_log =
        minera::import_xes_event_log(variant_input, options);
    assert(variant_log.metadata.extensions == log.metadata.extensions);
    assert(variant_log.metadata.classifiers == log.metadata.classifiers);
    assert(variant_log.metadata.trace_globals == log.metadata.trace_globals);
    assert(variant_log.metadata.event_globals == log.metadata.event_globals);
    assert(variant_log.metadata.properties == log.metadata.properties);
  }

  for (const auto variant : {minera::XesExportVariant::LINE_BY_LINE,
                             minera::XesExportVariant::ETREE}) {
    minera::XesExportOptions options;
    options.variant = variant;
    options.name = "fixture";
    const auto exported = minera::serialize_xes(log, options);
    std::istringstream round_input(exported);
    const auto round = minera::import_xes_event_log(round_input);
    assert(round.metadata.extensions == log.metadata.extensions);
    assert(round.metadata.classifiers == log.metadata.classifiers);
    assert(round.metadata.trace_globals == log.metadata.trace_globals);
    assert(round.metadata.event_globals == log.metadata.event_globals);
    assert(round.metadata.properties == log.metadata.properties);
    assert(round.attribute("concept:name") != nullptr);
    assert(*round.attribute("concept:name") == "fixture");
    assert(find_attribute(round.attributes, "source") != nullptr);
    assert(find_attribute(round.attributes, "version") != nullptr);
    assert(round.traces[0].attributes == log.traces[0].attributes);
    assert(round.traces[0].events[0].attributes ==
           log.traces[0].events[0].attributes);
    assert(minera::parse_xes_export_variant(
               minera::xes_export_variant_name(variant)) == variant);
  }
}

void stored_metadata_preserves_required_canonical_prefixes() {
  minera::EventLog log;
  log.metadata.extensions.push_back(
      {"Concept", "custom-concept", "https://example.test/concept"});
  log.metadata.classifiers.push_back({"Activity", {"custom activity"}});
  log.metadata.trace_globals.push_back(
      {"concept:name", "trace override", minera::AttributeKind::kString});
  log.metadata.event_globals.push_back(
      {"concept:name", "event override", minera::AttributeKind::kString});
  minera::Trace trace;
  trace.case_id = "case";
  trace.events.push_back({"case", "A"});
  log.traces.push_back(std::move(trace));

  const auto xml = minera::to_xes(log);
  assert(occurrences(xml, "<extension name=\"Concept\"") == 1);
  assert(occurrences(xml, "prefix=\"custom-concept\"") == 1);
  assert(occurrences(xml, "prefix=\"concept\"") == 0);
  assert(occurrences(xml, "<classifier name=\"Activity\"") == 1);
  assert(occurrences(xml, "key=\"concept:name\" value=\"trace override\"") ==
         1);
  assert(occurrences(xml, "key=\"concept:name\" value=\"event override\"") ==
         1);

  std::istringstream input(xml);
  const auto round = minera::import_xes_event_log(input);
  assert(round.metadata.extensions.front().name == "Concept");
  assert(round.metadata.extensions.front().prefix == "custom-concept");
  assert(round.metadata.classifiers.front().keys ==
         std::vector<std::string>{"custom activity"});
  assert(round.metadata.trace_globals.front().value == "trace override");
  assert(round.metadata.event_globals.front().value == "event override");

  minera::XesExportOptions inferred;
  inferred.infer_metadata = true;
  const auto inferred_xml = minera::serialize_xes(log, inferred);
  assert(occurrences(inferred_xml, "prefix=\"concept\"") == 1);
}

void explicit_nulls_round_trip_but_empty_strings_are_emitted() {
  minera::EventLog log;
  log.attributes.push_back(
      {"empty-log", "", minera::AttributeKind::kString, false});
  log.attributes.push_back(
      {"null-log", "not serialized", minera::AttributeKind::kString, true});
  log.metadata.trace_globals.push_back(
      {"empty-trace-global", "", minera::AttributeKind::kString, false});
  log.metadata.trace_globals.push_back(
      {"null-trace-global", "ignored", minera::AttributeKind::kString, true});
  log.metadata.trace_globals.push_back(
      {"concept:name", "ignored", minera::AttributeKind::kString, true});
  log.metadata.event_globals.push_back(
      {"empty-event-global", "", minera::AttributeKind::kString, false});
  minera::Trace trace;
  trace.case_id = "case";
  trace.attributes.push_back(
      {"empty-trace", "", minera::AttributeKind::kString, false});
  trace.attributes.push_back(
      {"null-trace", "ignored", minera::AttributeKind::kString, true});
  trace.events.push_back({"case", "A"});
  trace.events.front().attributes.push_back(
      {"empty-event", "", minera::AttributeKind::kString, false});
  trace.events.front().attributes.push_back(
      {"null-event", "ignored", minera::AttributeKind::kString, true});
  log.traces.push_back(std::move(trace));

  const auto xml = minera::to_xes(log);
  assert(xml.find("key=\"empty-log\" value=\"\"") != std::string::npos);
  assert(xml.find("key=\"empty-trace-global\" value=\"\"") !=
         std::string::npos);
  assert(xml.find("key=\"empty-event-global\" value=\"\"") !=
         std::string::npos);
  assert(xml.find("key=\"empty-trace\" value=\"\"") !=
         std::string::npos);
  assert(xml.find("key=\"empty-event\" value=\"\"") !=
         std::string::npos);
  assert(xml.find("key=\"null-log\" null=\"true\"") !=
         std::string::npos);
  assert(xml.find("key=\"null-trace-global\" null=\"true\"") !=
         std::string::npos);
  assert(occurrences(xml, "<global scope=\"trace\">") == 1);
  assert(occurrences(xml, "key=\"concept:name\" value=\"\"") == 0);
  assert(xml.find("key=\"null-trace\" null=\"true\"") !=
         std::string::npos);
  assert(xml.find("key=\"null-event\" null=\"true\"") !=
         std::string::npos);

  std::istringstream input(xml);
  const auto round = minera::import_xes_event_log(input);
  assert(find_attribute(round.attributes, "empty-log") != nullptr);
  assert(find_attribute(round.attributes, "null-log") != nullptr);
  assert(find_attribute(round.attributes, "null-log")->is_null);
  assert(find_attribute(round.traces.front().attributes, "empty-trace") !=
         nullptr);
  assert(find_attribute(round.traces.front().attributes, "null-trace") !=
         nullptr);
  assert(find_attribute(round.traces.front().attributes, "null-trace")->is_null);
  assert(find_attribute(round.traces.front().events.front().attributes,
                        "empty-event") != nullptr);
  assert(find_attribute(round.traces.front().events.front().attributes,
                        "null-event") != nullptr);
  assert(find_attribute(round.traces.front().events.front().attributes,
                        "null-event")->is_null);

  // Empty first-class values are present values too; they are emitted and
  // imported distinctly from a missing activity or case key.
  minera::EventLog empty_first_class;
  minera::Trace empty_trace;
  empty_trace.case_id = "";
  empty_trace.case_id_state = minera::FirstClassAttributeState::kPresent;
  empty_trace.events.push_back({"", ""});
  empty_trace.events.front().activity_state =
      minera::FirstClassAttributeState::kPresent;
  empty_first_class.traces.push_back(std::move(empty_trace));
  const auto empty_xml = minera::to_xes(empty_first_class);
  assert(empty_xml.find("<string key=\"concept:name\" value=\"\"/>") !=
         std::string::npos);
  std::istringstream empty_input(empty_xml);
  const auto empty_round = minera::import_xes_event_log(empty_input);
  assert(empty_round.traces.size() == 1);
  assert(empty_round.traces.front().case_id.empty());
  assert(empty_round.traces.front().events.front().activity.empty());
}

void generic_projection_aliases_are_not_dropped() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case";
  minera::Event event;
  event.case_id = "case";
  event.activity = "canonical activity";
  event.timestamp = "2024-01-02T03:04:05Z";
  event.resource = "canonical resource";
  event.lifecycle = "canonical lifecycle";
  trace.events.push_back(std::move(event));
  auto& attributes = trace.events.front().attributes;
  attributes.push_back(
      {"activity", "custom activity", minera::AttributeKind::kString});
  attributes.push_back(
      {"timestamp", "custom timestamp", minera::AttributeKind::kString});
  attributes.push_back(
      {"resource", "custom resource", minera::AttributeKind::kString});
  attributes.push_back(
      {"lifecycle", "custom lifecycle", minera::AttributeKind::kString});
  attributes.push_back(
      {"concept:name", "duplicate", minera::AttributeKind::kString});
  attributes.push_back(
      {"time:timestamp", "duplicate", minera::AttributeKind::kString});
  attributes.push_back(
      {"org:resource", "duplicate", minera::AttributeKind::kString});
  attributes.push_back({"lifecycle:transition", "duplicate",
                        minera::AttributeKind::kString});
  log.traces.push_back(std::move(trace));

  const auto xml = minera::to_xes(log);
  assert(xml.find("key=\"activity\" value=\"custom activity\"") !=
         std::string::npos);
  assert(xml.find("key=\"timestamp\" value=\"custom timestamp\"") !=
         std::string::npos);
  assert(xml.find("key=\"resource\" value=\"custom resource\"") !=
         std::string::npos);
  assert(xml.find("key=\"lifecycle\" value=\"custom lifecycle\"") !=
         std::string::npos);
  assert(xml.find("value=\"duplicate\"") != std::string::npos);
}

void typed_nested_attributes_and_first_class_states_round_trip() {
  const std::string xml =
      "<log>"
      "<string key=\"concept:name\" value=\"stored-name\"/>"
      "<global scope=\"custom\"><list key=\"tags\">"
      "<int key=\"0\" value=\"7\"/>"
      "<container key=\"details\"><boolean key=\"enabled\" value=\"true\"/>"
      "</container></list></global>"
      "<global scope=\"custom\"><string key=\"note\" value=\"second\"/>"
      "</global>"
      "<trace><event><int key=\"count\" value=\"42\"/>"
      "<float key=\"ratio\" value=\"1.25\"/>"
      "<boolean key=\"valid\" value=\"false\"/>"
      "<list key=\"labels\"><string key=\"0\" value=\"a\"/>"
      "<string key=\"1\" value=\"b\"/></list></event></trace></log>";
  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input);
  assert(log.attributes.size() == 1);
  assert(*log.attribute("concept:name") == "stored-name");
  assert(log.metadata.globals.size() == 2);
  assert(log.metadata.globals[0].scope == "custom");
  assert(log.metadata.globals[1].attributes[0].key == "note");
  assert(log.traces[0].case_id_state ==
         minera::FirstClassAttributeState::kMissing);
  const auto& event = log.traces[0].events[0];
  assert(event.activity_state == minera::FirstClassAttributeState::kMissing);
  const auto* count = find_attribute(event.attributes, "count");
  assert(count != nullptr && std::get<std::int64_t>(count->typed_value) == 42);
  const auto* labels = find_attribute(event.attributes, "labels");
  assert(labels != nullptr &&
         labels->container == minera::AttributeContainerKind::kList &&
         labels->children.size() == 2);
  assert(std::get<bool>(find_attribute(event.attributes, "valid")
                            ->typed_value) == false);

  const auto exported = minera::to_xes(log);
  std::istringstream round_input(exported);
  const auto round = minera::import_xes_event_log(round_input);
  assert(round.attribute("concept:name") != nullptr);
  assert(*round.attribute("concept:name") == "stored-name");
  assert(round.metadata.globals == log.metadata.globals);
  assert(round.traces[0].case_id_state ==
         minera::FirstClassAttributeState::kMissing);
  assert(round.traces[0].events[0].activity_state ==
         minera::FirstClassAttributeState::kMissing);
  assert(find_attribute(round.traces[0].events[0].attributes, "labels") !=
         nullptr);
}

void invalid_typed_values_are_rejected_explicitly() {
  for (const auto& scalar : {"<int key=\"x\" value=\"nope\"/>",
                             "<float key=\"x\" value=\"nope\"/>",
                             "<boolean key=\"x\" value=\"nope\"/>"}) {
    const auto xml = std::string("<log><trace><event><string key=\"concept:name\" value=\"A\"/>") +
                     scalar + "</event></trace></log>";
    assert(throws([&] {
      std::istringstream input(xml);
      (void)minera::import_xes_event_log(input);
    }));
  }
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c";
  trace.events.push_back({"c", "A"});
  trace.events.front().attributes.push_back(
      {"bad", "not-an-int", minera::AttributeKind::kInt});
  log.traces.push_back(std::move(trace));
  assert(throws([&] { (void)minera::to_xes(log); }));
}

void standards_lists_round_trip_recursive_typed_children() {
  const std::string xml =
      "<log><trace><string key=\"concept:name\" value=\"case\">"
      "<string key=\"case-meta\" value=\"kept\"/></string><event>"
      "<string key=\"concept:name\" value=\"A\">"
      "<int key=\"activity-meta\" value=\"3\"/></string>"
      "<list key=\"tags\"><values><int key=\"0\" value=\"7\"/>"
      "<string key=\"nested-meta\" value=\"outer\"><boolean "
      "key=\"enabled\" value=\"true\"/></string>"
      "<container key=\"details\"><date key=\"when\" "
      "value=\"2026-01-01T00:00:00Z\"/>"
      "<list key=\"labels\"><values><boolean key=\"0\" "
      "value=\"true\"/></values></list></container>"
      "</values></list>"
      "<string key=\"nested-null\" null=\"true\"><int key=\"count\" "
      "value=\"2\"/></string></event></trace></log>";
  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input);
  assert(log.traces[0].case_id == "case");
  const auto* selected_case =
      find_attribute(log.traces[0].attributes, "concept:name");
  assert(selected_case != nullptr && selected_case->children.size() == 1);
  const auto* selected_activity = find_attribute(
      log.traces[0].events[0].attributes, "concept:name");
  assert(selected_activity != nullptr &&
         selected_activity->children.size() == 1);
  const auto* tags = find_attribute(log.traces[0].events[0].attributes, "tags");
  assert(tags != nullptr && tags->container == minera::AttributeContainerKind::kList);
  assert(tags->children.size() == 3);
  assert(std::get<std::int64_t>(tags->children[0].typed_value) == 7);
  assert(tags->children[1].key == "nested-meta");
  assert(tags->children[1].children.size() == 1);
  assert(std::get<bool>(tags->children[1].children[0].typed_value));
  assert(tags->children[2].children.size() == 2);
  assert(std::get<minera::NativeDate>(tags->children[2].children[0].typed_value)
             .value == "2026-01-01T00:00:00.000Z");
  assert(std::get<bool>(tags->children[2].children[1].children[0].typed_value));
  const auto* nested_null =
      find_attribute(log.traces[0].events[0].attributes, "nested-null");
  assert(nested_null != nullptr && nested_null->is_null &&
         nested_null->children.size() == 1);
  assert(std::get<std::int64_t>(nested_null->children[0].typed_value) == 2);

  const auto exported = minera::to_xes(log);
  assert(exported.find("<list key=\"tags\">") != std::string::npos);
  assert(exported.find("<values>") != std::string::npos);
  assert(exported.find("<list key=\"labels\">") != std::string::npos);
  assert(exported.find("<string key=\"nested-meta\" value=\"outer\">") !=
         std::string::npos);
  assert(exported.find("<string key=\"nested-null\" null=\"true\">") !=
         std::string::npos);
  assert(exported.find("key=\"activity-meta\" value=\"3\"") !=
         std::string::npos);
  assert(exported.find("key=\"case-meta\" value=\"kept\"") !=
         std::string::npos);
  std::istringstream round_input(exported);
  const auto round = minera::import_xes_event_log(round_input);
  assert(round.traces[0].events[0].attributes ==
         log.traces[0].events[0].attributes);
  assert(round.traces[0].attributes == log.traces[0].attributes);
}

void stale_lexical_values_are_not_written_and_invalid_dates_fail() {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "case";
  minera::Event event;
  event.activity = "A";
  minera::EventAttribute integer{"integer", "stale", minera::AttributeKind::kInt};
  integer.typed_value = std::int64_t{9};
  event.attributes.push_back(std::move(integer));
  minera::EventAttribute boolean{"boolean", "stale", minera::AttributeKind::kBoolean};
  boolean.typed_value = true;
  event.attributes.push_back(std::move(boolean));
  trace.events.push_back(std::move(event));
  log.traces.push_back(std::move(trace));
  const auto exported = minera::to_xes(log);
  assert(exported.find("<int key=\"integer\" value=\"9\"/>") !=
         std::string::npos);
  assert(exported.find("<boolean key=\"boolean\" value=\"true\"/>") !=
         std::string::npos);

  minera::EventAttribute invalid{"invalid-date", "not-a-date",
                                 minera::AttributeKind::kDate};
  log.traces[0].events[0].attributes.push_back(std::move(invalid));
  assert(throws([&] { (void)minera::to_xes(log); }));
  assert(throws([] {
    std::istringstream input(
        "<log><trace><event><date key=\"when\" value=\"not-a-date\"/>"
        "</event></trace></log>");
    (void)minera::import_xes_event_log(input);
  }));
}

void metadata_legacy_global_edits_are_exported() {
  const std::string xml =
      "<log><global scope=\"trace\"><string key=\"trace-a\" "
      "value=\"A\"/></global><global scope=\"custom\"><string "
      "key=\"custom-x\" value=\"X\"/></global><global scope=\"trace\">"
      "<string key=\"trace-b\" value=\"B\"/></global><trace><string "
      "key=\"concept:name\" value=\"case\"/><event><string "
      "key=\"concept:name\" value=\"A\"/></event></trace></log>";
  std::istringstream input(xml);
  auto log = minera::import_xes_event_log(input);
  assert(log.metadata.globals.size() == 3);
  assert(log.metadata.trace_globals.size() == 2);
  log.metadata.trace_globals[0].value = "A-edited";
  const auto exported = minera::to_xes(log);
  const auto edited_position = exported.find("key=\"trace-a\" value=\"A-edited\"");
  const auto custom_position = exported.find("key=\"custom-x\" value=\"X\"");
  const auto second_position = exported.find("key=\"trace-b\" value=\"B\"");
  assert(edited_position != std::string::npos);
  assert(custom_position != std::string::npos);
  assert(second_position != std::string::npos);
  assert(edited_position < custom_position && custom_position < second_position);
  assert(occurrences(exported, "<global scope=\"trace\">") == 2);
  assert(occurrences(exported, "<global scope=\"custom\">") == 1);

  minera::EventLog manual;
  manual.metadata.globals = {
      {"trace", {{"manual-trace", "kept"}}},
      {"custom", {{"manual-custom", "kept"}}},
      {"event", {{"manual-event", "kept"}}},
  };
  const auto manual_xml = minera::to_xes(manual);
  assert(manual_xml.find("key=\"manual-trace\" value=\"kept\"") !=
         std::string::npos);
  assert(manual_xml.find("key=\"manual-event\" value=\"kept\"") !=
         std::string::npos);
  assert(manual_xml.find("key=\"manual-custom\" value=\"kept\"") !=
         std::string::npos);
}

void attribute_equality_is_an_equivalence_relation() {
  minera::EventAttribute legacy{"n", "1", minera::AttributeKind::kInt};
  minera::EventAttribute typed = legacy;
  typed.typed_value = std::int64_t{1};
  assert(legacy == legacy);
  assert(typed == typed);
  assert(!(legacy == typed));
  minera::EventAttribute nan_a{"nan", "NaN", minera::AttributeKind::kFloat};
  nan_a.typed_value = std::numeric_limits<double>::quiet_NaN();
  auto nan_b = nan_a;
  assert(nan_a == nan_b);
}

void deeply_nested_xes_attributes_are_rejected() {
  std::string xml = "<log><trace><event>";
  constexpr std::size_t nesting = 140;
  for (std::size_t i = 0; i < nesting; ++i)
    xml += "<list key=\"nested\"><values>";
  xml += "<string key=\"leaf\" value=\"x\"/>";
  for (std::size_t i = 0; i < nesting; ++i) xml += "</values></list>";
  xml += "</event></trace></log>";
  assert(throws([&] {
    std::istringstream input(xml);
    (void)minera::import_xes_event_log(input);
  }));
}

void conflicting_projection_aliases_are_retained() {
  const std::string xml =
      "<log><trace><string key=\"concept:name\" value=\"case-canonical\"/>"
      "<string key=\"case_id\" value=\"case-alias\"/>"
      "<event><string key=\"concept:name\" value=\"activity-canonical\"/>"
      "<string key=\"activity\" value=\"activity-alias\"/>"
      "<string key=\"time:timestamp\" value=\"canonical-time\"/>"
      "<string key=\"timestamp\" value=\"alias-time\"/>"
      "</event></trace></log>";
  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input);
  const auto& trace = log.traces.front();
  const auto& event = trace.events.front();
  assert(trace.case_id == "case-canonical");
  assert(trace.case_id_source_key == "concept:name");
  assert(*trace.attribute("case_id") == "case-alias");
  assert(event.activity == "activity-canonical");
  assert(event.activity_source_key == "concept:name");
  assert(*event.attribute("activity") == "activity-alias");
  assert(event.timestamp == "canonical-time");
  assert(*event.attribute("timestamp") == "alias-time");

  minera::XesImportOptions custom;
  custom.activity_attribute = "activity";
  custom.timestamp_attribute = "timestamp";
  std::istringstream custom_input(xml);
  const auto custom_log = minera::import_xes_event_log(custom_input, custom);
  assert(custom_log.traces.front().events.front().activity == "activity-alias");
  assert(custom_log.traces.front().events.front().activity_source_key ==
         "activity");
  assert(*custom_log.traces.front().events.front().attribute("concept:name") ==
         "activity-canonical");
}

void inferred_metadata_completes_empty_and_partial_globals() {
  minera::EventLog empty;
  minera::XesExportOptions options;
  options.infer_metadata = true;
  const auto empty_xml = minera::serialize_xes(empty, options);
  assert(occurrences(empty_xml, "prefix=\"concept\"") == 1);
  assert(occurrences(empty_xml, "<global scope=\"trace\">") == 1);
  assert(occurrences(empty_xml, "<global scope=\"event\">") == 1);
  assert(occurrences(empty_xml, "key=\"concept:name\" value=\"\"") == 2);
  assert(empty_xml.find("<classifier name=\"Activity\"") !=
         std::string::npos);
  assert(empty_xml.find("key=\"concept:name\" value=\"log\"") !=
         std::string::npos);

  minera::EventLog partial;
  partial.metadata.globals = {
      {"custom", {{"custom:first", "one"}}},
      {"trace", {{"trace:first", "two"}}},
      {"custom", {{"custom:second", "three"}}},
      {"event", {{"concept:name", "already"}}},
  };
  const auto partial_xml = minera::serialize_xes(partial, options);
  // Existing declarations and their attributes retain document order while
  // absent trace/event concept defaults are appended as new declarations.
  const auto custom_first = partial_xml.find("custom:first");
  const auto trace_first = partial_xml.find("trace:first");
  const auto custom_second = partial_xml.find("custom:second");
  const auto event_name = partial_xml.find("value=\"already\"");
  assert(custom_first < trace_first && trace_first < custom_second &&
         custom_second < event_name);
  assert(occurrences(partial_xml, "<global scope=\"trace\">") == 2);
  assert(occurrences(partial_xml, "<global scope=\"event\">") == 1);
  assert(partial_xml.find("key=\"concept:name\" value=\"\"") !=
         std::string::npos);

  // Inference remains opt-in: the same empty log has no generated defaults.
  minera::XesExportOptions plain;
  const auto plain_xml = minera::serialize_xes(empty, plain);
  assert(plain_xml.find("prefix=\"concept\"") == std::string::npos);
  assert(plain_xml.find("<classifier name=\"Activity\"") ==
         std::string::npos);
  assert(plain_xml.find("<global") == std::string::npos);
}

void null_containers_keep_recursive_children() {
  const std::string xml =
      "<log><trace><event><list key=\"tags\" null=\"true\"><values>"
      "<int key=\"0\" value=\"7\"/></values></list>"
      "<container key=\"payload\" null=\"true\"><string key=\"note\" "
      "value=\"kept\"/></container></event></trace></log>";
  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input);
  const auto* tags =
      find_attribute(log.traces[0].events[0].attributes, "tags");
  const auto* payload =
      find_attribute(log.traces[0].events[0].attributes, "payload");
  assert(tags != nullptr && tags->is_null && tags->children.size() == 1);
  assert(payload != nullptr && payload->is_null && payload->children.size() == 1);
  const auto exported = minera::to_xes(log);
  assert(exported.find("<list key=\"tags\" null=\"true\">") !=
         std::string::npos);
  assert(exported.find("<container key=\"payload\" null=\"true\">") !=
         std::string::npos);
  assert(exported.find("key=\"0\" value=\"7\"") != std::string::npos);
  assert(exported.find("key=\"note\" value=\"kept\"") !=
         std::string::npos);
  std::istringstream round_input(exported);
  const auto round = minera::import_xes_event_log(round_input);
  const auto* round_tags =
      find_attribute(round.traces[0].events[0].attributes, "tags");
  assert(round_tags != nullptr && round_tags->is_null &&
         round_tags->children.size() == 1);
}

void typed_first_class_selectors_survive_flat_adapters() {
  const std::string xml =
      "<log><trace><int key=\"case.number\" value=\"42\"><string "
      "key=\"case-meta\" value=\"kept\"/></int><event><int "
      "key=\"activity.code\" value=\"7\"><string key=\"activity-meta\" "
      "value=\"nested\"/></int><date key=\"when.value\" "
      "value=\"2026-01-01T00:00:00Z\"><int key=\"precision\" "
      "value=\"3\"/></date></event></trace></log>";
  minera::XesImportOptions import_options;
  import_options.case_attribute = "case.number";
  import_options.activity_attribute = "activity.code";
  import_options.timestamp_attribute = "when.value";
  std::istringstream input(xml);
  const auto log = minera::import_xes_event_log(input, import_options);
  assert(log.traces[0].case_id == "42");
  assert(log.traces[0].case_id_attribute);
  assert(log.traces[0].case_id_attribute->kind == minera::AttributeKind::kInt);
  assert(std::get<std::int64_t>(log.traces[0].case_id_attribute->typed_value) ==
         42);
  assert(log.traces[0].case_id_attribute->children.size() == 1);
  const auto& event = log.traces[0].events[0];
  assert(event.activity == "7" && event.activity_attribute);
  assert(event.activity_attribute->kind == minera::AttributeKind::kInt);
  assert(std::get<std::int64_t>(event.activity_attribute->typed_value) == 7);
  assert(event.activity_attribute->children.size() == 1);
  assert(event.timestamp_attribute &&
         event.timestamp_attribute->kind == minera::AttributeKind::kDate);

  const auto direct = minera::to_xes(log);
  assert(direct.find("<int key=\"activity.code\" value=\"7\">") !=
         std::string::npos);
  assert(direct.find("<int key=\"case.number\" value=\"42\">") !=
         std::string::npos);

  const auto stream = minera::event_log_to_event_stream(log);
  const auto* activity = stream.events[0].field("activity.code");
  const auto* case_id = stream.events[0].field("case:concept:name");
  assert(activity != nullptr && activity->first_class_role ==
                                    minera::FirstClassAttributeRole::kActivity);
  assert(activity->kind == minera::AttributeKind::kInt &&
         std::get<std::int64_t>(activity->typed_value) == 7 &&
         activity->children.size() == 1);
  assert(case_id != nullptr && case_id->kind == minera::AttributeKind::kInt &&
         std::get<std::int64_t>(case_id->typed_value) == 42 &&
         case_id->children.size() == 1);
  const auto table = minera::event_stream_to_table(stream);
  const auto restored = minera::event_table_to_event_log(table);
  assert(restored.traces[0].case_id == "42");
  assert(restored.traces[0].case_id_source_key == "case.number");
  assert(restored.traces[0].case_id_attribute &&
         restored.traces[0].case_id_attribute->kind ==
             minera::AttributeKind::kInt);
  assert(restored.traces[0].events[0].activity == "7");
  assert(restored.traces[0].events[0].activity_source_key == "activity.code");
  assert(restored.traces[0].events[0].activity_attribute &&
         restored.traces[0].events[0].activity_attribute->kind ==
             minera::AttributeKind::kInt);
  assert(restored.traces[0].events[0].activity_attribute->children.size() == 1);
}

void set_values_export_as_strings() {
  using namespace minera;
  EventLog log;
  log.traces.resize(1);
  log.traces[0].case_id = "case";
  log.traces[0].events.resize(1);
  auto& event = log.traces[0].events[0];
  event.activity = "A";
  EventAttribute set{"set", "ignored"};
  set.container = AttributeContainerKind::kSet;
  set.children = {{"", "2", AttributeKind::kInt}, {"", "1", AttributeKind::kInt}};
  auto frozen = set;
  frozen.key = "frozen";
  frozen.container = AttributeContainerKind::kFrozenSet;
  auto empty = frozen;
  empty.key = "empty";
  empty.children.clear();
  event.attributes = {set, frozen, empty};
  for (const auto variant : {XesExportVariant::kLineByLine, XesExportVariant::kElementTree}) {
    XesExportOptions options;
    options.variant = variant;
    std::istringstream input(serialize_xes(log, options));
    const auto restored = import_xes_event_log(input);
    const auto& attributes = restored.traces[0].events[0].attributes;
    const std::vector<std::string> expected = {"{2, 1}", "frozenset({2, 1})", "frozenset()"};
    assert(attributes.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      assert(attributes[i].kind == AttributeKind::kString);
      assert(attributes[i].container == AttributeContainerKind::kScalar);
      assert(attributes[i].children.empty());
      assert(attributes[i].value == expected[i]);
    }
  }
  const auto table_copy = event_table_to_event_log(event_log_to_table(log));
  assert(table_copy.traces[0].events[0].attributes == event.attributes);
}

void date_import_variations_retain_wall_clock_fields() {
  using namespace minera;
  for (const auto variant : {XesImportVariant::ITERPARSE, XesImportVariant::LINE_BY_LINE,
      XesImportVariant::ITERPARSE_MEM_COMPRESSED, XesImportVariant::ITERPARSE_20,
      XesImportVariant::CHUNK_REGEX}) {
    for (const auto* base : {"2024-01-01T00:00:00", "1969-12-31T23:59:59"}) {
      for (const auto* fraction : {".123456", ".123456789"}) {
        for (const auto* zone : {"", "Z", "+00:00", "+05:30", "-04:00"}) {
          const auto source = std::string(base) + fraction + zone;
          const auto expected = std::string(base) + ".123456Z";
          const auto date = "<date key=\"when\" value=\"" + source + "\"/>";
          const auto xml = "<log>" + date + "<trace><string key=\"concept:name\" value=\"c\"/>" +
              date + "<string key=\"nested\" value=\"outer\">" + date + "</string><event>" +
              "<string key=\"concept:name\" value=\"A\"/>" + date +
              "<string key=\"nested\" value=\"outer\">" + date + "</string>" +
              "<date key=\"generic\" value=\"" + source + "\"/>" +
              "<string key=\"lexical\" value=\"" + source + "\"/></event></trace></log>";
          for (const bool compressed : {false, true}) {
            XesImportOptions options;
            options.variant = variant;
            options.timestamp_attribute = "when";
            std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
            const auto log = import_xes_event_log(input, options);
            const auto& trace = log.traces[0];
            const auto& event = trace.events[0];
            assert(log.attributes[0].value == expected);
            assert(find_attribute(trace.attributes, "when")->value == expected);
            assert(find_attribute(trace.attributes, "nested")->children[0].value == expected);
            assert(event.timestamp == expected);
            assert(std::get<NativeDate>(event.timestamp_attribute->typed_value).value == expected);
            assert(find_attribute(event.attributes, "generic")->value == expected);
            assert(find_attribute(event.attributes, "nested")->children[0].value == expected);
            assert(find_attribute(event.attributes, "lexical")->value == source);
          }
        }
      }
    }
  }
}

void date_import_sorting_uses_exact_fields_and_variant_rules() {
  using namespace minera;
  const std::string xml = R"(<log><trace><string key="concept:name" value="c"/>
    <event><string key="concept:name" value="late"/><date key="time:timestamp" value="2500-01-01T00:00:00.000002Z"/></event>
    <event><string key="concept:name" value="early"/><date key="time:timestamp" value="2500-01-01T00:00:00.000001Z"/></event>
    </trace></log>)";
  for (const auto variant : {XesImportVariant::ITERPARSE, XesImportVariant::LINE_BY_LINE,
      XesImportVariant::ITERPARSE_MEM_COMPRESSED, XesImportVariant::ITERPARSE_20,
      XesImportVariant::CHUNK_REGEX}) {
    for (const bool reverse : {false, true}) {
      XesImportOptions options;
      options.variant = variant;
      options.timestamp_sort = true;
      options.reverse_sort = reverse;
      std::istringstream input(xml);
      const auto log = import_xes_event_log(input, options);
      const bool late_first = reverse || variant == XesImportVariant::CHUNK_REGEX;
      assert(log.traces[0].events[0].activity == (late_first ? "late" : "early"));
      assert(log.traces[0].events[1].activity == (late_first ? "early" : "late"));
    }
  }
  XesImportOptions rust;
  rust.variant = XesImportVariant::RUSTXES;
  std::istringstream input(R"(<log><trace><string key="concept:name" value="c"/>
    <event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00.123456789+05:30"/></event>
    <event><string key="concept:name" value="B"/><date key="time:timestamp" value="2024-01-01T00:00:00.123456-04:00"/></event>
    </trace></log>)");
  const auto log = import_xes_event_log(input, rust);
  assert(log.traces[0].events[0].timestamp == "2023-12-31T18:30:00.123456Z");
  assert(log.traces[0].events[1].timestamp == "2024-01-01T04:00:00.123456Z");
}

void date_export_preserves_source_precision() {
  using namespace minera;
  const std::vector<std::pair<std::string, std::string>> dates{
      {"2024-01-01T00:00:00.000001Z", "2024-01-01T00:00:00.000001Z"},
      {"2024-01-01T00:00:00.000999Z", "2024-01-01T00:00:00.000999Z"},
      {"2024-01-01T00:00:00.001001Z", "2024-01-01T00:00:00.001001Z"},
      {"2024-01-01T00:00:00.123456Z", "2024-01-01T00:00:00.123456Z"},
      {"1969-12-31T23:59:59.999999Z", "1969-12-31T23:59:59.999999Z"},
      {"2500-01-01T00:00:00.000001Z", "2500-01-01T00:00:00.000001Z"},
      {"2024-01-01T00:00:00.123456+05:30", "2023-12-31T18:30:00.123456Z"},
      {"2024-01-01T00:00:00.999999-04:00", "2024-01-01T04:00:00.999999Z"}};
  for (const auto& [source, expected] : dates) {
    EventAttribute date{"date", "stale", AttributeKind::kDate};
    date.typed_value = NativeDate{source};
    EventLog log;
    log.attributes = {date};
    Trace trace{"c", {}};
    trace.attributes = {date};
    Event event{"c", "A", "stale", "r"};
    event.timestamp_attribute = date;
    event.timestamp_attribute->key = "time:timestamp";
    event.timestamp_state = FirstClassAttributeState::kPresent;
    EventAttribute nested{"nested", ""};
    nested.container = AttributeContainerKind::kContainer;
    nested.children = {date};
    event.attributes = {date, nested, {"legacy", source, AttributeKind::kDate}};
    trace.events = {event};
    log.traces = {trace};
    for (const auto variant : {XesExportVariant::LINE_BY_LINE, XesExportVariant::ETREE}) {
      for (const bool compressed : {false, true}) {
        XesExportOptions options;
        options.variant = variant;
        options.compress = compressed;
        const auto bytes = serialize_xes(log, options);
        if (!compressed) assert(occurrences(bytes, "value=\"" + expected + "\"") == 6);
        std::istringstream input(bytes);
        const auto restored = import_xes_event_log(input);
        const auto& restored_event = restored.traces[0].events[0];
        assert(restored_event.timestamp == expected);
        assert(restored.attributes[0].value == expected);
        assert(restored.traces[0].attributes[0].value == expected);
        assert(find_attribute(restored_event.attributes, "date")->value == expected);
        assert(find_attribute(restored_event.attributes, "legacy")->value == expected);
        assert(find_attribute(restored_event.attributes, "nested")->children[0].value == expected);
      }
    }
  }
  EventLog precise;
  precise.traces.push_back(Trace{"c", {Event{"c", "A"}}});
  EventAttribute nanos{"precise", "stale", AttributeKind::kDate};
  nanos.typed_value = NativeDate{"1969-12-31T23:59:59.999999999Z", NativeDateResolution::kNanosecond};
  precise.traces[0].events[0].attributes = {nanos};
  for (const auto variant : {XesExportVariant::LINE_BY_LINE, XesExportVariant::ETREE}) {
    XesExportOptions options;
    options.variant = variant;
    const auto xml = serialize_xes(precise, options);
    assert(xml.find("1969-12-31T23:59:59.999999999Z") != std::string::npos);
    std::istringstream input(xml);
    const auto restored = import_xes_event_log(input);
    assert(restored.traces[0].events[0].attributes[0].value == "1969-12-31T23:59:59.999999Z");
  }
}

}  // namespace

int main() {
  date_import_sorting_uses_exact_fields_and_variant_rules();
  date_import_variations_retain_wall_clock_fields();
  date_export_preserves_source_precision();
  {
    std::istringstream input(R"(<log><trace><string key="concept:name" value="c"/>
      <event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00.000999Z"/></event>
      <event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00.001001Z"/></event>
      <event><string key="concept:name" value="A"/><date key="time:timestamp" value="2024-01-01T00:00:00.123456789Z"/></event>
      <event><string key="concept:name" value="A"/><date key="time:timestamp" value="1969-12-31T23:59:59.999999Z"/></event>
      </trace></log>)");
    const auto imported = minera::import_xes_event_log(input);
    const std::vector<std::string> expected{
      "2024-01-01T00:00:00.000999Z", "2024-01-01T00:00:00.001001Z",
      "2024-01-01T00:00:00.123456Z", "1969-12-31T23:59:59.999999Z"};
    for (std::size_t i = 0; i < expected.size(); ++i) {
      const auto& event = imported.traces[0].events[i];
      assert(event.timestamp == expected[i]);
      assert(std::get<minera::NativeDate>(event.timestamp_attribute->typed_value).value == expected[i]);
    }
  }
  set_values_export_as_strings();
  variants_and_gzip_round_trip();
  imports_external_dynamic_huffman_gzip();
  imports_fixed_huffman_and_concatenated_gzip();
  rejects_corruption_and_expansion_beyond_limit();
  file_export_uses_gzip_suffix_and_imports_it();
  metadata_import_export_import_round_trip();
  stored_metadata_preserves_required_canonical_prefixes();
  explicit_nulls_round_trip_but_empty_strings_are_emitted();
  generic_projection_aliases_are_not_dropped();
  typed_nested_attributes_and_first_class_states_round_trip();
  invalid_typed_values_are_rejected_explicitly();
  standards_lists_round_trip_recursive_typed_children();
  stale_lexical_values_are_not_written_and_invalid_dates_fail();
  metadata_legacy_global_edits_are_exported();
  attribute_equality_is_an_equivalence_relation();
  deeply_nested_xes_attributes_are_rejected();
  conflicting_projection_aliases_are_retained();
  inferred_metadata_completes_empty_and_partial_globals();
  null_containers_keep_recursive_children();
  typed_first_class_selectors_survive_flat_adapters();
  return 0;
}
