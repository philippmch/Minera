#include <cassert>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "minera/xes_event_log_importer.hpp"
#include "minera/xes_export.hpp"
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
      "<log xes.version=\"1.0\" xes.features=\"\" "
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
  assert(log.metadata.properties.size() == 5);
  assert(log.metadata.properties[0].key == "case_attribute");
  assert(log.metadata.properties[0].value == "concept:name");
  assert(log.metadata.properties[1].key == "activity_attribute");
  assert(log.metadata.properties[2].key == "timestamp_attribute");
  assert(log.metadata.properties[3].key == "resource_attribute");
  assert(log.metadata.properties[4].key == "lifecycle_attribute");
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
    assert(round.attributes == log.attributes);
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
  assert(occurrences(xml, "<extension name=\"Concept\"") == 2);
  assert(occurrences(xml, "prefix=\"custom-concept\"") == 1);
  assert(occurrences(xml, "prefix=\"concept\"") == 1);
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
}

void explicit_nulls_are_omitted_but_empty_strings_are_emitted() {
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
  assert(xml.find("null-log") == std::string::npos);
  assert(xml.find("null-trace-global") == std::string::npos);
  assert(occurrences(xml, "<global scope=\"trace\">") == 1);
  assert(occurrences(xml, "key=\"concept:name\" value=\"\"") >= 1);
  assert(xml.find("null-trace") == std::string::npos);
  assert(xml.find("null-event") == std::string::npos);

  std::istringstream input(xml);
  const auto round = minera::import_xes_event_log(input);
  assert(find_attribute(round.attributes, "empty-log") != nullptr);
  assert(find_attribute(round.attributes, "null-log") == nullptr);
  assert(find_attribute(round.traces.front().attributes, "empty-trace") !=
         nullptr);
  assert(find_attribute(round.traces.front().attributes, "null-trace") ==
         nullptr);
  assert(find_attribute(round.traces.front().events.front().attributes,
                        "empty-event") != nullptr);
  assert(find_attribute(round.traces.front().events.front().attributes,
                        "null-event") == nullptr);

  // Empty first-class values are present values too; they are emitted and
  // imported distinctly from a missing activity or case key.
  minera::EventLog empty_first_class;
  minera::Trace empty_trace;
  empty_trace.case_id = "";
  empty_trace.events.push_back({"", ""});
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
  assert(xml.find("value=\"duplicate\"") == std::string::npos);
}

}  // namespace

int main() {
  variants_and_gzip_round_trip();
  imports_external_dynamic_huffman_gzip();
  imports_fixed_huffman_and_concatenated_gzip();
  rejects_corruption_and_expansion_beyond_limit();
  file_export_uses_gzip_suffix_and_imports_it();
  metadata_import_export_import_round_trip();
  stored_metadata_preserves_required_canonical_prefixes();
  explicit_nulls_are_omitted_but_empty_strings_are_emitted();
  generic_projection_aliases_are_not_dropped();
  return 0;
}
