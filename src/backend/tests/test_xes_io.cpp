#include <cassert>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <array>
#include <cstdint>
#include <cstdio>
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
#include "../root_cause_trace_key.hpp"

namespace {

bool throws(const auto& operation) {
  try {
    operation();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

std::string separate_xes_tags(std::string xml) {
  std::size_t at = 0;
  while ((at = xml.find("><", at)) != std::string::npos) {
    xml.insert(at + 1, "\n");
    at += 3;
  }
  return xml;
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

// Compressed exports must round-trip, and with zlib they are deflated at level 9
// (as Python's gzip writes them) rather than stored.
void compresses_gzip_exports() {
  std::string repetitive;
  for (int i = 0; i < 2000; ++i)
    repetitive += "<event><string key=\"concept:name\" value=\"Approve\"/></event>\n";
  const auto compressed = minera::detail::gzip_compress(repetitive);
  assert(minera::detail::is_gzip_data(compressed));
  assert(minera::detail::gzip_decompress(compressed, repetitive.size()) == repetitive);
  assert(minera::detail::gzip_decompress(minera::detail::gzip_compress(""), 0).empty());
#if defined(MINERA_HAS_ZLIB)
  assert(compressed.size() * 20 < repetitive.size());
#endif
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
  // The limit errors name the bound and how to raise it, plain or gzip.
  const auto limit_message = [&](const std::string& bytes) {
    try {
      std::istringstream input(bytes);
      (void)minera::import_xes_event_log(input, bounded);
    } catch (const std::exception& error) {
      return std::string(error.what());
    }
    return std::string();
  };
  const auto plain = limit_message(minera::serialize_xes(sample_log(), minera::XesExportOptions{}));
  assert(plain.starts_with("XES input exceeds the XES size limit of 16 bytes"));
  assert(plain.find("--max-xes-decompressed-bytes") != std::string::npos);
  const auto compressed = valid;  // a gzip export
  // The gzip header names no operating system (0xff), on every platform.
  assert(compressed.size() > 10 && static_cast<unsigned char>(compressed[9]) == 0xff);
  const auto gzip = limit_message(compressed);
  assert(gzip.starts_with("gzip XES input expands past the XES size limit of 16 bytes"));
  assert(throws([&] { (void)minera::detail::gzip_decompress(compressed, 16); }));
  bool limit_type = false;
  try {
    (void)minera::detail::gzip_decompress(compressed, 16);
  } catch (const minera::detail::GzipOutputLimitError&) {
    limit_type = true;
  }
  assert(limit_type);
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
    if (variant == minera::XesImportVariant::LINE_BY_LINE) {
      assert(variant_log.metadata.extensions.empty());
      assert(variant_log.metadata.classifiers.empty());
      assert(variant_log.metadata.globals.empty());
      assert(variant_log.attributes.empty());
      continue;
    }
    if (variant == minera::XesImportVariant::RUSTXES) {
      assert(variant_log.metadata.classifiers.empty());
      assert(variant_log.metadata.globals.empty());
      assert(variant_log.metadata.trace_globals.empty());
      assert(variant_log.metadata.event_globals.empty());
      assert(variant_log.attributes == std::vector<minera::EventAttribute>({{"origin", "csv"}}));
      continue;
    }
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

  const auto xml = minera::serialize_xes(log, {.preserve_nulls = true});
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
      "<global scope=\"custom2\"><string key=\"note\" value=\"second\"/>"
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
  // Public iterparse drops a <container> wherever it appears.
  assert(log.metadata.globals[0].attributes[0].children.size() == 1);
  assert(log.metadata.globals[1].attributes[0].key == "note");
  assert(log.traces[0].case_id_state ==
         minera::FirstClassAttributeState::kMissing);
  const auto& event = log.traces[0].events[0];
  assert(event.activity_state == minera::FirstClassAttributeState::kMissing);
  const auto* count = find_attribute(event.attributes, "count");
  assert(count != nullptr && std::get<std::int64_t>(count->typed_value) == 42);
  const auto* labels = find_attribute(event.attributes, "labels");
  assert(labels != nullptr &&
         labels->container == minera::AttributeContainerKind::kContainer &&
         labels->children.size() == 2);
  assert(std::get<bool>(find_attribute(event.attributes, "valid")
                            ->typed_value) == false);

  minera::XesExportOptions options;
  options.variant = minera::XesExportVariant::ETREE;
  const auto exported = minera::serialize_xes(log, options);
  std::istringstream round_input(exported);
  const auto round = minera::import_xes_event_log(round_input);
  assert(round.attribute("concept:name") != nullptr);
  assert(*round.attribute("concept:name") == "stored-name");
  auto expected_globals = log.metadata.globals;
  expected_globals[0].attributes[0].container = minera::AttributeContainerKind::kList;
  assert(round.metadata.globals == expected_globals);
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
                             "<date key=\"x\" value=\"2020-02-30T10:00:00Z\"/>"}) {
    const auto xml = std::string("<log><trace><event><string key=\"concept:name\" value=\"A\"/>") +
                     scalar + "</event></trace></log>";
    // The iterparse readers (the default) drop just that attribute.
    for (const auto variant : {minera::XesImportVariant::kIterparse,
                               minera::XesImportVariant::kIterparse20,
                               minera::XesImportVariant::kIterparseMemoryCompressed}) {
      std::istringstream input(xml);
      minera::XesImportOptions options;
      options.variant = variant;
      const auto log = minera::import_xes_event_log(input, options);
      assert(log.traces.size() == 1 && log.traces[0].events.size() == 1);
      assert(log.traces[0].events[0].activity == "A");
      assert(find_attribute(log.traces[0].events[0].attributes, "x") == nullptr);
    }
    // The regex reader raises, as the reference does.
    assert(throws([&] {
      std::istringstream input(xml);
      minera::XesImportOptions options;
      options.variant = minera::XesImportVariant::kChunkRegex;
      (void)minera::import_xes_event_log(input, options);
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
      "<list key=\"details\"><values><date key=\"when\" "
      "value=\"2026-01-01T00:00:00Z\"/>"
      "<list key=\"labels\"><values><boolean key=\"0\" "
      "value=\"true\"/></values></list></values></list>"
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

  const auto exported = minera::serialize_xes(log, {.preserve_nulls = true});
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
  // The default iterparse reader drops the unreadable date and keeps the
  // event; the regex reader raises, as the reference does.
  const std::string bad_date =
      "<log><trace><event><date key=\"when\" value=\"not-a-date\"/>"
      "</event></trace></log>";
  {
    std::istringstream input(bad_date);
    const auto read = minera::import_xes_event_log(input);
    assert(read.traces[0].events.size() == 1 &&
           read.traces[0].events[0].attributes.empty());
  }
  assert(throws([&] {
    std::istringstream input(bad_date);
    minera::XesImportOptions options;
    options.variant = minera::XesImportVariant::kChunkRegex;
    (void)minera::import_xes_event_log(input, options);
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
  // The iterparse readers keep the last declaration of a repeated scope, in
  // the place of the first.
  assert(log.metadata.globals.size() == 2);
  assert(log.metadata.globals[0].scope == "trace" &&
         log.metadata.globals[0].attributes[0].key == "trace-b");
  assert(log.metadata.trace_globals.size() == 1 &&
         log.metadata.trace_globals[0].key == "trace-b");
  // A log that does carry a repeated scope (built natively) keeps both
  // declarations when its legacy projection is edited.
  log.metadata.globals = {{"trace", {{"trace-a", "A"}}},
                          {"custom", {{"custom-x", "X"}}},
                          {"trace", {{"trace-b", "B"}}}};
  log.metadata.trace_globals = {{"trace-a", "A"}, {"trace-b", "B"}};
  log.metadata.compatibility_snapshot_valid = true;
  log.metadata.compatibility_globals_snapshot = log.metadata.globals;
  log.metadata.compatibility_trace_globals_snapshot = log.metadata.trace_globals;
  log.metadata.compatibility_event_globals_snapshot = log.metadata.event_globals;
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
  auto log = minera::import_xes_event_log(input);
  const auto* tags =
      find_attribute(log.traces[0].events[0].attributes, "tags");
  assert(tags != nullptr && tags->is_null && tags->children.size() == 1);
  // Public iterparse drops the <container>; a native one is still written.
  assert(find_attribute(log.traces[0].events[0].attributes, "payload") == nullptr);
  minera::EventAttribute payload{"payload", "", minera::AttributeKind::kString, true};
  payload.container = minera::AttributeContainerKind::kContainer;
  payload.children = {{"note", "kept"}};
  log.traces[0].events[0].attributes.push_back(payload);
  const auto exported = minera::serialize_xes(log, {.preserve_nulls = true});
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

void null_scalar_list_children_preserve_order_and_duplicates() {
  using namespace minera;
  EventLog log;
  EventAttribute ordered{"probe", "true", AttributeKind::kBoolean, true};
  ordered.scalar_children_list = true;
  ordered.children = {
      {"same", "first", AttributeKind::kString},
      {"same", "2", AttributeKind::kInt},
  };
  log.attributes.push_back(ordered);

  XesExportOptions preserved_options;
  preserved_options.variant = XesExportVariant::ETREE;
  preserved_options.preserve_nulls = true;
  const auto preserved = serialize_xes(log, preserved_options);
  assert(occurrences(preserved, "key=\"same\"") == 2);
  assert(preserved.find("value=\"first\"") < preserved.find("value=\"2\""));
  assert(preserved.find("key=\"probe\" null=\"true\"") != std::string::npos);

  for (const auto variant : {XesExportVariant::ETREE, XesExportVariant::LINE_BY_LINE}) {
    XesExportOptions options;
    options.variant = variant;
    const auto default_nulls = serialize_xes(log, options);
    assert(default_nulls.find("null=\"true\"") == std::string::npos);
    assert(occurrences(default_nulls, "key=\"same\"") == 2);
    assert(default_nulls.find("value=\"first\"") < default_nulls.find("value=\"2\""));
  }

  XesExportOptions line_preserved = preserved_options;
  line_preserved.variant = XesExportVariant::LINE_BY_LINE;
  assert(throws([&] { (void)serialize_xes(log, line_preserved); }));
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

#include "xes_nested_expected.inc"

void nested_attribute_variations_match_public_categories() {
  using namespace minera;
  for (const auto& scenario : xes_nested_scenarios) {
    XesImportOptions options;
    options.variant = parse_xes_import_variant(scenario.variant);
    std::istringstream input(scenario.xml);
    const auto log = import_xes_event_log(input, options);
    assert(log.traces.size() == 1 && log.traces[0].events.size() == 1);
    const auto& trace = log.traces[0];
    const auto& event = trace.events[0];
    std::vector<EventAttribute> actual = std::string_view(scenario.scope) == "trace"
        ? trace.attributes : event.attributes;
    const auto add_selected = [&](const std::optional<EventAttribute>& attribute) {
      if (attribute && !find_attribute(actual, attribute->key)) actual.push_back(*attribute);
    };
    if (std::string_view(scenario.scope) == "trace") add_selected(trace.case_id_attribute);
    else {
      add_selected(event.activity_attribute);
      add_selected(event.timestamp_attribute);
      add_selected(event.resource_attribute);
      add_selected(event.lifecycle_attribute);
    }
    if (actual.size() != scenario.attributes.size()) {
      std::fprintf(stderr, "nested shape size: %s expected %zu got %zu\n", scenario.name, scenario.attributes.size(), actual.size());
      assert(false);
    }
    for (const auto& [key, expected, type] : scenario.attributes) {
      const auto* attribute = find_attribute(actual, key);
      const auto category = attribute ? root_cause_attribute_category(*attribute) : "<missing>";
      if (category != expected) {
        std::fprintf(stderr, "nested shape: %s key %s expected %s got %s\n", scenario.name, key.c_str(), expected.c_str(), category.c_str());
        assert(false);
      }
      assert(attribute);
      if (type == "str") assert(attribute->kind == AttributeKind::kString);
      else if (type == "int") assert(std::holds_alternative<std::int64_t>(attribute->typed_value));
      else if (type == "float") assert(std::holds_alternative<double>(attribute->typed_value));
      else if (type == "bool") assert(std::holds_alternative<bool>(attribute->typed_value));
      else if (type == "datetime") assert(std::holds_alternative<NativeDate>(attribute->typed_value));
      else assert(type == "mapping" && (attribute->container != AttributeContainerKind::kScalar || !attribute->children.empty()));
    }
  }
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
            const auto formatted = variant == XesImportVariant::LINE_BY_LINE ? separate_xes_tags(xml) : xml;
            std::istringstream input(compressed ? detail::gzip_compress(formatted) : formatted);
            const auto log = import_xes_event_log(input, options);
            const auto& trace = log.traces[0];
            const auto& event = trace.events[0];
            if (variant == XesImportVariant::LINE_BY_LINE) assert(log.attributes.empty());
            else assert(log.attributes[0].value == expected);
            assert(find_attribute(trace.attributes, "when")->value == expected);
            const bool flat = variant == XesImportVariant::LINE_BY_LINE || variant == XesImportVariant::CHUNK_REGEX;
            const auto& trace_children = find_attribute(trace.attributes, "nested")->children;
            assert(flat ? trace_children.empty() : trace_children[0].value == expected);
            assert(event.timestamp == expected);
            assert(std::get<NativeDate>(event.timestamp_attribute->typed_value).value == expected);
            assert(find_attribute(event.attributes, "generic")->value == expected);
            const auto& event_children = find_attribute(event.attributes, "nested")->children;
            assert(flat ? event_children.empty() : event_children[0].value == expected);
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
      std::istringstream input(variant == XesImportVariant::LINE_BY_LINE ? separate_xes_tags(xml) : xml);
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

void nested_export_variants_match_public_mapping_shapes() {
  using namespace minera;
  const auto check = [](const EventAttribute& parent, const std::string& line,
                        const std::string& tree, bool line_error = false) {
    for (const auto variant : {XesExportVariant::LINE_BY_LINE, XesExportVariant::ETREE}) {
      for (const bool compressed : {false, true}) {
        for (int scope = 0; scope != 6; ++scope) {
          EventLog log;
          log.traces = {Trace{"c", {Event{"c", "A"}}}};
          auto value = parent;
          if (scope == 0) log.attributes = {value};
          if (scope == 1) log.metadata.globals.push_back({"global", {value}});
          if (scope == 2) log.traces[0].attributes = {value};
          if (scope == 3) log.traces[0].events[0].attributes = {value};
          if (scope == 4) {
            value.key = "concept:name";
            log.traces[0].case_id = value.value;
            log.traces[0].case_id_state = FirstClassAttributeState::kPresent;
            log.traces[0].case_id_attribute = value;
          }
          if (scope == 5) {
            value.key = "concept:name";
            log.traces[0].events[0].activity = value.value;
            log.traces[0].events[0].activity_state = FirstClassAttributeState::kPresent;
            log.traces[0].events[0].activity_attribute = value;
          }
          XesExportOptions options;
          options.variant = variant;
          options.compress = compressed;
          if (line_error && variant == XesExportVariant::LINE_BY_LINE) {
            assert(throws([&] { (void)serialize_xes(log, options); }));
            continue;
          }
          const auto bytes = serialize_xes(log, options);
          const auto xml = compressed ? detail::gzip_decompress(bytes, 1024 * 1024) : bytes;
          const auto begin = xml.find("key=\"" + value.key + "\"", scope == 5 ? xml.find("<event>") : 0);
          assert(begin != std::string::npos);
          const auto end = xml.find(parent.container == AttributeContainerKind::kScalar ? "</string>" : "</list>", begin);
          assert(end != std::string::npos);
          auto actual = xml.substr(begin, end - begin);
          std::string compact;
          for (const char c : actual) if (c != ' ' && c != '\n' && c != '\r' && c != '\t') compact += c;
          const auto& expected = variant == XesExportVariant::LINE_BY_LINE ? line : tree;
          const auto scalar_value = parent.container == AttributeContainerKind::kScalar
              ? "value=\"" + value.value + "\"" : "";
          assert(compact == "key=\"" + value.key + "\"" + scalar_value + ">" + expected);
        }
      }
    }
  };
  EventAttribute mapping{"parent", ""};
  mapping.container = AttributeContainerKind::kContainer;
  check(mapping, "<values></values>", "<values></values>");
  mapping.children = {{"ab", "7", AttributeKind::kInt}, {"cd", "text"}};
  check(mapping, "<values><stringkey=\"a\"value=\"b\"/><stringkey=\"c\"value=\"d\"/></values>",
        "<values><intkey=\"ab\"value=\"7\"/><stringkey=\"cd\"value=\"text\"/></values>");
  mapping.children = {{"\xF0\x9F\x98\x80x", "7", AttributeKind::kInt}, {"\xC3\xA9\xE6\xBC\xA2", "text"}};
  check(mapping, "<values><stringkey=\"\xF0\x9F\x98\x80\"value=\"x\"/><stringkey=\"\xC3\xA9\"value=\"\xE6\xBC\xA2\"/></values>",
        "<values><intkey=\"\xF0\x9F\x98\x80x\"value=\"7\"/><stringkey=\"\xC3\xA9\xE6\xBC\xA2\"value=\"text\"/></values>");
  for (const std::string key : {"a", "\xF0\x9F\x98\x80", "\xC3\xA9", ""}) {
    mapping.children = {{key, "7", AttributeKind::kInt}};
    // The reference line writer fails on a key shorter than two characters;
    // Minera writes that child as itself.
    const std::string whole = "<values><intkey=\"" + key + "\"value=\"7\"/></values>";
    check(mapping, whole, whole);
  }
  mapping.children = {{"ab", "1", AttributeKind::kInt}, {"cd", "text"}, {"ab", "9", AttributeKind::kInt}};
  check(mapping, "<values><stringkey=\"a\"value=\"b\"/><stringkey=\"c\"value=\"d\"/></values>",
        "<values><intkey=\"ab\"value=\"9\"/><stringkey=\"cd\"value=\"text\"/></values>");
  mapping.container = AttributeContainerKind::kList;
  const std::string sequence = "<values><intkey=\"ab\"value=\"1\"/><stringkey=\"cd\"value=\"text\"/><intkey=\"ab\"value=\"9\"/></values>";
  check(mapping, sequence, sequence);
  mapping.container = AttributeContainerKind::kScalar;
  mapping.value = "outer";
  const std::string scalar_children = "<intkey=\"ab\"value=\"9\"/><stringkey=\"cd\"value=\"text\"/>";
  check(mapping, scalar_children, scalar_children);
  EventAttribute nested{"nested", ""};
  nested.container = AttributeContainerKind::kContainer;
  nested.children = {{"ab", "7", AttributeKind::kInt}};
  mapping.children = {nested};
  check(mapping, "<listkey=\"nested\"><values><stringkey=\"a\"value=\"b\"/></values></list>",
        "<listkey=\"nested\"><values><intkey=\"ab\"value=\"7\"/></values></list>");
}

void boolean_import_spelling_matches_public_variants() {
  using namespace minera;
  for (const auto variant : {XesImportVariant::ITERPARSE, XesImportVariant::LINE_BY_LINE,
      XesImportVariant::ITERPARSE_MEM_COMPRESSED, XesImportVariant::ITERPARSE_20,
      XesImportVariant::CHUNK_REGEX, XesImportVariant::RUSTXES}) {
    for (const std::string lexical : {"true", "false", "True", "False", "TRUE",
        "FALSE", "tRuE", "1", "0", "yes", "", " true "}) {
      const std::string xml = "<log>\n<trace>\n<string key=\"concept:name\" value=\"c\"/>\n<boolean key=\"flag\" value=\"" + lexical +
          "\"/>\n<event>\n<boolean key=\"flag\" value=\"" +
          lexical + "\"/>\n</event>\n</trace>\n</log>";
      const bool exact = variant == XesImportVariant::LINE_BY_LINE ||
          variant == XesImportVariant::CHUNK_REGEX || variant == XesImportVariant::RUSTXES;
      const bool expected = lexical == "true" || (!exact &&
          (lexical == "True" || lexical == "TRUE" || lexical == "tRuE"));
      for (const bool compressed : {false, true}) {
        const auto bytes = compressed ? detail::gzip_compress(xml) : xml;
        std::istringstream input(bytes);
        XesImportOptions options;
        options.variant = variant;
        const auto log = import_xes_event_log(input, options);
        const auto* attribute = find_attribute(log.traces.at(0).events.at(0).attributes, "flag");
        assert(attribute != nullptr);
        assert(std::get<bool>(attribute->typed_value) == expected);
        assert(attribute->value == lexical);
        const auto* trace_attribute = find_attribute(log.traces.at(0).attributes, "flag");
        assert(trace_attribute != nullptr);
        assert(std::get<bool>(trace_attribute->typed_value) == expected);
        assert(trace_attribute->value == lexical);
      }
    }
  }
}

void boolean_export_spelling_matches_variant_and_scalar_shape() {
  using namespace minera;
  for (const bool truth : {false, true}) {
    for (int source = 0; source != 3; ++source) {
      for (const bool nested : {false, true}) {
        EventAttribute value{"flag", source == 0 ? "stale" :
            source == 1 ? (truth ? "true" : "false") : (truth ? "1" : "0"),
            AttributeKind::kBoolean};
        if (source == 0) value.typed_value = truth;
        if (nested) {
          EventAttribute child{"child", truth ? "0" : "1", AttributeKind::kBoolean};
          EventAttribute branch{"branch", value.value, AttributeKind::kBoolean};
          branch.typed_value = value.typed_value;
          branch.children = {child};
          value.children = {child, branch};
        }
        for (int scope = 0; scope != 9; ++scope) {
          EventLog log;
          log.traces = {Trace{"c", {Event{"c", "A"}}}};
          auto selected = value;
          auto& trace = log.traces[0];
          auto& event = trace.events[0];
          if (scope == 0) log.attributes = {value};
          if (scope == 1) log.metadata.globals.push_back({"event", {value}});
          if (scope == 2) trace.attributes = {value};
          if (scope == 3) event.attributes = {value};
          if (scope >= 4) {
            const std::string keys[] = {"concept:name", "concept:name", "time:timestamp",
                                        "org:resource", "lifecycle:transition"};
            selected.key = keys[scope - 4];
            std::string* projections[] = {&trace.case_id, &event.activity, &event.timestamp,
                                          &event.resource, &event.lifecycle};
            FirstClassAttributeState* states[] = {&trace.case_id_state, &event.activity_state,
                &event.timestamp_state, &event.resource_state, &event.lifecycle_state};
            std::optional<EventAttribute>* metadata[] = {&trace.case_id_attribute,
                &event.activity_attribute, &event.timestamp_attribute, &event.resource_attribute,
                &event.lifecycle_attribute};
            *projections[scope - 4] = value.value;
            *states[scope - 4] = FirstClassAttributeState::kPresent;
            *metadata[scope - 4] = selected;
          }
          for (const auto variant : {XesExportVariant::LINE_BY_LINE, XesExportVariant::ETREE}) {
            for (const bool compressed : {false, true}) {
              XesExportOptions options;
              options.variant = variant;
              options.compress = compressed;
              const auto bytes = serialize_xes(log, options);
              const auto xml = compressed ? detail::gzip_decompress(bytes, 1024 * 1024) : bytes;
              const std::string expected = nested && variant == XesExportVariant::ETREE
                  ? (truth ? "True" : "False") : (truth ? "true" : "false");
              assert(occurrences(xml, "<boolean key=\"" + selected.key + "\" value=\"" +
                  expected + (nested ? "\">" : "\"/>")) == 1);
              assert(xml.find("stale") == std::string::npos);
              if (nested) {
                assert(occurrences(xml, std::string("<boolean key=\"child\" value=\"") +
                    (truth ? "false" : "true") + "\"/>") == 2);
                assert(occurrences(xml, "<boolean key=\"branch\" value=\"" + expected + "\">") == 1);
              }
              if (scope < 4) {
                std::istringstream input(bytes);
                const auto restored = import_xes_event_log(input);
                const auto& attributes = scope == 0 ? restored.attributes :
                    scope == 1 ? restored.metadata.globals.at(0).attributes :
                    scope == 2 ? restored.traces.at(0).attributes :
                    restored.traces.at(0).events.at(0).attributes;
                const auto* flag = find_attribute(attributes, "flag");
                assert(flag != nullptr && std::get<bool>(flag->typed_value) == truth);
                if (nested) {
                  const auto* child = find_attribute(flag->children, "child");
                  const auto* branch = find_attribute(flag->children, "branch");
                  assert(child && std::get<bool>(child->typed_value) == !truth);
                  assert(branch && std::get<bool>(branch->typed_value) == truth);
                }
              }
            }
          }
        }
      }
    }
  }
}

void date_export_preserves_source_precision() {
  using namespace minera;
  const std::vector<std::pair<std::string, std::string>> dates{
      {"2024-01-01T00:00:00", "2024-01-01T00:00:00"},
      {"2024-01-01T00:00:00.000000Z", "2024-01-01T00:00:00+00:00"},
      {"2024-01-01T00:00:00.1Z", "2024-01-01T00:00:00.100000+00:00"},
      {"2024-01-01T00:00:00.000001Z", "2024-01-01T00:00:00.000001+00:00"},
      {"2024-01-01T00:00:00.000999Z", "2024-01-01T00:00:00.000999+00:00"},
      {"2024-01-01T00:00:00.001001Z", "2024-01-01T00:00:00.001001+00:00"},
      {"2024-01-01T00:00:00.123456789Z", "2024-01-01T00:00:00.123456+00:00"},
      {"1969-12-31T23:59:59.999999Z", "1969-12-31T23:59:59.999999+00:00"},
      {"2500-01-01T00:00:00.000001Z", "2500-01-01T00:00:00.000001+00:00"},
      {"0001-01-01T00:00:00Z", "0001-01-01T00:00:00+00:00"},
      {"2024-01-01T00:00:00.123456+05:30", "2024-01-01T00:00:00.123456+05:30"},
      {"2024-01-01T00:00:00.999999-04:00", "2024-01-01T00:00:00.999999-04:00"}};
  for (const auto& [source, expected] : dates) {
    EventAttribute date{"date", "stale", AttributeKind::kDate};
    date.typed_value = NativeDate{source};
    const auto fraction_end = expected.find_first_of("+-", 19);
    const auto wall_clock = expected.substr(0, fraction_end);
    const auto imported = wall_clock + (wall_clock.size() == 19 ? ".000Z" : "Z");
    EventLog log;
    log.attributes = {date};
    Trace trace{"c", {}};
    trace.attributes = {date};
    Event event{"c", "A", "stale", "r"};
    event.timestamp_attribute = date;
    event.timestamp_attribute->key = "time:timestamp";
    event.timestamp_state = FirstClassAttributeState::kPresent;
    EventAttribute nested{"nested", "outer"};
    nested.children = {date};
    auto wrapper = date;
    wrapper.key = "wrapper";
    wrapper.children = {{"child", "metadata"}};
    event.attributes = {date, nested, {"legacy", source, AttributeKind::kDate}, wrapper};
    trace.events = {event};
    log.traces = {trace};
    for (const auto variant : {XesExportVariant::LINE_BY_LINE, XesExportVariant::ETREE}) {
      for (const bool compressed : {false, true}) {
        XesExportOptions options;
        options.variant = variant;
        options.compress = compressed;
        const auto bytes = serialize_xes(log, options);
        if (!compressed) {
          const bool etree = variant == XesExportVariant::ETREE;
          assert(occurrences(bytes, "value=\"" + expected + "\"") == (etree ? 6 : 7));
          auto wrapper_value = expected;
          if (etree) wrapper_value[10] = ' ';
          assert(bytes.find("key=\"wrapper\" value=\"" + wrapper_value + "\"") != std::string::npos);
        }
        std::istringstream input(bytes);
        const auto restored = import_xes_event_log(input);
        const auto& restored_event = restored.traces[0].events[0];
        assert(restored_event.timestamp == imported);
        assert(restored.attributes[0].value == imported);
        assert(restored.traces[0].attributes[0].value == imported);
        assert(find_attribute(restored_event.attributes, "date")->value == imported);
        assert(find_attribute(restored_event.attributes, "legacy")->value == imported);
        assert(find_attribute(restored_event.attributes, "nested")->children[0].value == imported);
      }
    }
  }
  for (const auto& [source, expected] : std::vector<std::pair<std::string, std::string>>{
      {"2024-01-01T00:00:00.000000000", "2024-01-01T00:00:00"},
      {"2024-01-01T00:00:00.1Z", "2024-01-01T00:00:00.100000+00:00"},
      {"2024-01-01T00:00:00.123456000Z", "2024-01-01T00:00:00.123456+00:00"},
      {"2024-01-01T00:00:00.000000001+05:30", "2024-01-01T00:00:00.000000001+05:30"},
      {"1969-12-31T23:59:59.999999999-04:00", "1969-12-31T23:59:59.999999999-04:00"},
      {"2500-01-01T00:00:00.000000001Z", "2500-01-01T00:00:00.000000001+00:00"}}) {
    EventLog precise;
    precise.traces.push_back(Trace{"c", {Event{"c", "A"}}});
    EventAttribute nanos{"precise", "stale", AttributeKind::kDate};
    nanos.typed_value = NativeDate{source, NativeDateResolution::kNanosecond};
    auto wrapper = nanos;
    wrapper.key = "wrapper";
    wrapper.children = {{"child", "metadata"}};
    precise.traces[0].events[0].attributes = {nanos, wrapper};
    for (const auto variant : {XesExportVariant::LINE_BY_LINE, XesExportVariant::ETREE}) {
      XesExportOptions options;
      options.variant = variant;
      const auto xml = serialize_xes(precise, options);
      assert(xml.find("key=\"precise\" value=\"" + expected + "\"") != std::string::npos);
      auto wrapper_value = expected;
      if (variant == XesExportVariant::ETREE) wrapper_value[10] = ' ';
      assert(xml.find("key=\"wrapper\" value=\"" + wrapper_value + "\"") != std::string::npos);
    }
  }
}

}  // namespace

void unsigned_integer_boundaries_match_public_variants() {
  using namespace minera;
  for (const auto value : {std::uint64_t{9223372036854775808ULL},
                           (std::numeric_limits<std::uint64_t>::max)()}) {
    const auto xml = separate_xes_tags(
        "<log><trace><string key=\"concept:name\" value=\"c\"/>"
        "<event><string key=\"concept:name\" value=\"A\"/>"
        "<int key=\"amount\" value=\"" + std::to_string(value) +
        "\"/></event></trace></log>");
    for (const auto variant : {XesImportVariant::ITERPARSE, XesImportVariant::LINE_BY_LINE,
         XesImportVariant::ITERPARSE_MEM_COMPRESSED, XesImportVariant::ITERPARSE_20,
         XesImportVariant::CHUNK_REGEX, XesImportVariant::RUSTXES}) {
      std::istringstream input(xml);
      XesImportOptions options;
      options.variant = variant;
      const auto log = import_xes_event_log(input, options);
      const auto* amount = find_attribute(log.traces.at(0).events.at(0).attributes, "amount");
      assert(amount && amount->kind == AttributeKind::kInt);
      if (variant == XesImportVariant::RUSTXES) {
        assert(std::get<std::int64_t>(amount->typed_value) == 0);
      } else {
        assert(std::get<std::uint64_t>(amount->typed_value) == value);
        for (const auto export_variant : {XesExportVariant::ETREE, XesExportVariant::LINE_BY_LINE}) {
          const auto exported = serialize_xes(log, {.variant = export_variant});
          assert(exported.find(std::to_string(value)) != std::string::npos);
          std::istringstream reinput(exported);
          const auto round = import_xes_event_log(reinput);
          const auto* restored = find_attribute(round.traces.at(0).events.at(0).attributes, "amount");
          assert(restored && std::get<std::uint64_t>(restored->typed_value) == value);
        }
      }
    }
  }
}

// Reference captures from pm4py 2.7.22 read_xes (ITERPARSE, its default),
// observed in fresh processes with PYTHONHASHSEED 1-3 (review F3 fixtures).
minera::EventLog read_iterparse(const std::string& xml) {
  std::istringstream input(xml);
  return minera::import_xes_event_log(input);
}

std::string one_event(const std::string& attributes) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<log xes.version=\"1.0\"><trace>"
         "<string key=\"concept:name\" value=\"c\"/><event>"
         "<string key=\"concept:name\" value=\"A\"/>" + attributes +
         "</event></trace></log>";
}

void iterparse_reference_typed_values() {
  // int(): whitespace, underscores, leading zeros and Unicode digits.
  const std::pair<const char*, std::int64_t> ints[] = {
      {"1_000", 1000}, {" 3", 3}, {"3 ", 3}, {"-0", 0}, {"\xd9\xa3", 3}, {"007", 7}};
  for (const auto& [text, expected] : ints) {
    const auto log = read_iterparse(one_event(std::string("<int key=\"n\" value=\"") + text + "\"/>"));
    const auto* n = find_attribute(log.traces[0].events[0].attributes, "n");
    assert(n != nullptr && std::get<std::int64_t>(n->typed_value) == expected);
    assert(n->value == std::to_string(expected));
  }
  // float(): inf/nan spellings, underscores, whitespace, overflow to inf.
  const std::pair<const char*, double> floats[] = {
      {"1_0.5", 10.5}, {" 2.5", 2.5}, {".5", 0.5}, {"5.", 5.0}, {"1E5", 100000.0},
      {"inf", std::numeric_limits<double>::infinity()},
      {"Infinity", std::numeric_limits<double>::infinity()},
      {"-inf", -std::numeric_limits<double>::infinity()},
      {"1e400", std::numeric_limits<double>::infinity()}};
  for (const auto& [text, expected] : floats) {
    const auto log = read_iterparse(one_event(std::string("<float key=\"f\" value=\"") + text + "\"/>"));
    const auto* f = find_attribute(log.traces[0].events[0].attributes, "f");
    assert(f != nullptr && std::get<double>(f->typed_value) == expected);
  }
  for (const char* text : {"nan", "+NaN", "-NaN"}) {
    const auto log = read_iterparse(one_event(std::string("<float key=\"f\" value=\"") + text + "\"/>"));
    const auto* f = find_attribute(log.traces[0].events[0].attributes, "f");
    assert(f != nullptr && std::isnan(std::get<double>(f->typed_value)));
  }
  // Dropped attribute, event kept.
  for (const char* scalar : {"<int key=\"x\" value=\"1.5\"/>", "<int key=\"x\" value=\"0x10\"/>",
                             "<int key=\"x\" value=\"\"/>", "<float key=\"x\" value=\"0x1p3\"/>",
                             "<float key=\"x\" value=\"abc\"/>", "<int key=\"x\" value=\"1__0\"/>"}) {
    const auto log = read_iterparse(one_event(scalar));
    assert(log.traces[0].events.size() == 1);
    assert(find_attribute(log.traces[0].events[0].attributes, "x") == nullptr);
  }
  // A malformed value is dropped with its children, at every level.
  const auto nested = read_iterparse(
      "<log><int key=\"logbad\" value=\"x\"/><trace><string key=\"concept:name\" value=\"c\"/>"
      "<int key=\"tbad\" value=\"q\"/><event><string key=\"concept:name\" value=\"A\"/>"
      "<int key=\"n\" value=\"zz\"><string key=\"child\" value=\"v\"/></int>"
      "<date key=\"time:timestamp\" value=\"bad\"/><float key=\"f\" value=\"1.5\"/></event>"
      "<event><string key=\"concept:name\" value=\"B\"/><list key=\"L\"><values>"
      "<int key=\"i\" value=\"bad\"/><int key=\"j\" value=\"2\"/></values></list></event>"
      "</trace></log>");
  assert(nested.attributes.empty());
  assert(nested.traces[0].attributes.empty());
  const auto& first = nested.traces[0].events[0];
  assert(!first.has_timestamp() && first.attributes.size() == 1 && first.attributes[0].key == "f");
  const auto* list = find_attribute(nested.traces[0].events[1].attributes, "L");
  assert(list != nullptr && list->children.size() == 1 && list->children[0].key == "j");
  // Booleans: only a case-insensitive "true" is true.
  for (const auto& [text, expected] : std::initializer_list<std::pair<const char*, bool>>{
           {"tRuE", true}, {"1", false}, {"yes", false}, {" true", false}, {"", false}}) {
    const auto log = read_iterparse(one_event(std::string("<boolean key=\"b\" value=\"") + text + "\"/>"));
    assert(std::get<bool>(find_attribute(log.traces[0].events[0].attributes, "b")->typed_value) == expected);
  }
}

void iterparse_reference_dates() {
  // fromisoformat forms; the clock fields are kept and UTC attached.
  const std::pair<const char*, const char*> accepted[] = {
      {"2020-01-01", "2020-01-01T00:00:00.000Z"},
      {"2020-01-01T10:00", "2020-01-01T10:00:00.000Z"},
      {"2020-01-01 10:00:00", "2020-01-01T10:00:00.000Z"},
      {"2020-01-01T10:00:00+0200", "2020-01-01T10:00:00.000Z"},
      {"2020-01-01T10:00:00,5Z", "2020-01-01T10:00:00.500Z"},
      {"2020-01-01T10:00:00.123+14:00", "2020-01-01T10:00:00.123Z"},
      {"20200101T100000Z", "2020-01-01T10:00:00.000Z"},
      {"2020-W01-1", "2019-12-30T00:00:00.000Z"},
      {"2020-01-01T10:00:00.Z", "2020-01-01T10:00:00.000Z"},
      {"2020-01-01T10:00:00+05:30:15", "2020-01-01T10:00:00.000Z"},
      {"2020-01-01T10Z", "2020-01-01T10:00:00.000Z"},
      {"1970-01-01T00:00:00.0000001Z", "1970-01-01T00:00:00.000000Z"},
      {"2020-01-01T10:00:00.123456789123Z", "2020-01-01T10:00:00.123456Z"},
      {"0001-01-01T00:00:00Z", "0001-01-01T00:00:00.000Z"},
      {"9999-12-31T23:59:59.999999Z", "9999-12-31T23:59:59.999999Z"}};
  for (const auto& [text, expected] : accepted) {
    const auto log = read_iterparse(
        one_event(std::string("<date key=\"time:timestamp\" value=\"") + text + "\"/>"));
    const auto& event = log.traces[0].events[0];
    assert(event.has_timestamp());
    assert(event.timestamp == expected);
  }
  for (const char* text : {"2020-01-01T10:00:00.5z", "2020-01-01T24:00:00Z", "2020-02-30T10:00:00Z",
                           "garbage", "2020-01-01T10:00:60Z", " 2020-01-01T10:00:00Z",
                           "2020-1-1T10:00:00Z", "2020-01-01T10:00:00+25:00",
                           "10000-01-01T00:00:00Z", "2020-01-01T10:00:00 UTC", ""}) {
    const auto log = read_iterparse(
        one_event(std::string("<date key=\"time:timestamp\" value=\"") + text + "\"/>"));
    assert(log.traces[0].events.size() == 1 && !log.traces[0].events[0].has_timestamp());
  }
}

void iterparse_reference_rejects_malformed_xml() {
  const std::string ok_event =
      "<event><string key=\"concept:name\" value=\"A\"/></event>";
  const std::string head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  const std::vector<std::string> rejected = {
      "",
      "hello world",
      head + "<log><trace><string key=\"concept:name\" value=\"c1\"/>" + ok_event + "</trace>\n<trace>",
      head + "<log><trace><string key=\"concept:name\" value=\"c1\"/>" + ok_event + "</trace>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A\"/></evnt></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A\"/>" + ok_event + "</event></trace></log>",
      head + "<trace>" + ok_event + "</trace>",
      head + "<foo><trace>" + ok_event + "</trace></foo>",
      head + "<LOG><TRACE>" + ok_event + "</TRACE></LOG>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A & B\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A < B\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A\" value=\"B\"/></event></trace></log>",
      head + "<log><trace>" + ok_event + "</trace></log>garbage",
      head + "<log><trace>" + ok_event + "</trace></log><log/>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A&#1;B\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A\x01" "B\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A&#xD800;B\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"A&#x110000;B\"/></event></trace></log>",
      // ISO-8859-1 bytes: the reader is UTF-8 whatever the declaration says.
      "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<log><trace><event>"
      "<string key=\"concept:name\" value=\"caf\xe9\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"\xff\xfe\"/></event></trace></log>",
      "<?xml version=\"1.0\"?>\n<!DOCTYPE log [<!ENTITY xxe SYSTEM \"file:///etc/hostname\">]>\n"
      "<log><trace><event><string key=\"concept:name\" value=\"&xxe;\"/></event></trace></log>",
      head + "<log><trace><event><string key=\"concept:name\" value=\"&undefined;\"/></event></trace></log>",
  };
  for (const auto& xml : rejected) {
    for (const auto variant : {minera::XesImportVariant::kIterparse,
                               minera::XesImportVariant::kIterparse20,
                               minera::XesImportVariant::kIterparseMemoryCompressed}) {
      minera::XesImportOptions options;
      options.variant = variant;
      assert(throws([&] {
        std::istringstream input(xml);
        (void)minera::import_xes_event_log(input, options);
      }));
    }
  }
  // Entity amplification is refused without expanding the whole document.
  std::string lol = "<?xml version=\"1.0\"?>\n<!DOCTYPE lolz [\n<!ENTITY lol \"lol\">\n";
  for (int level = 1; level <= 9; ++level) {
    lol += "<!ENTITY lol" + std::to_string(level) + " \"";
    for (int copy = 0; copy < 10; ++copy)
      lol += level == 1 ? std::string("&lol;") : "&lol" + std::to_string(level - 1) + ";";
    lol += "\">\n";
  }
  lol += "]>\n<log><trace><event><string key=\"concept:name\" value=\"&lol9;\"/></event></trace></log>";
  assert(throws([&] { (void)read_iterparse(lol); }));
  // lxml's limit: 256 nested elements are read, 257 are not.
  const auto nested = [](std::size_t lists) {
    std::string xml = "<log><trace><string key=\"concept:name\" value=\"c1\"/><event>"
                      "<string key=\"concept:name\" value=\"A\"/>";
    for (std::size_t i = 0; i < lists; ++i) xml += "<list key=\"l\">";
    for (std::size_t i = 0; i < lists; ++i) xml += "</list>";
    return xml + "</event></trace></log>";
  };
  assert(read_iterparse(nested(253)).traces[0].events[0].activity == "A");
  assert(throws([&] { (void)read_iterparse(nested(254)); }));
}

// pm4py 2.7.22 iterparse readers on misplaced <trace>/<event> starts and XML
// declarations (lxml), with the reference's messages.
void iterparse_reference_rejects_misplaced_elements_and_declarations() {
  const std::string event = "<event><string key=\"concept:name\" value=\"A\"/></event>";
  const std::string trace = "<trace><string key=\"concept:name\" value=\"c1\"/>" + event + "</trace>";
  const std::string head = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  const std::string trace_in_trace = "file contains <trace> in another <trace> tag";
  const std::string event_in_event = "file contains <event> in another <event> tag";
  const std::string late_declaration = "XML declaration allowed only at the start of the document";
  const std::vector<std::pair<std::string, std::string>> rejected = {
      {head + "<log><trace><string key=\"concept:name\" value=\"c1\"/><trace>" + event +
           "</trace></trace></log>", trace_in_trace},
      {head + "<log><trace><event><trace>" + event + "</trace></event></trace></log>", trace_in_trace},
      // An <event> directly under <log> stays open, so any later <event> fails.
      {head + "<log>" + event + trace + "</log>", event_in_event},
      {head + "<log>" + event + event + "</log>", event_in_event},
      {" " + head + "<log>" + trace + "</log>", late_declaration},
      {"\n" + head + "<log>" + trace + "</log>", late_declaration},
      {"<!-- c -->" + head + "<log>" + trace + "</log>", late_declaration},
      {"<?XML version=\"1.0\"?>\n<log>" + trace + "</log>", "Invalid PI name"},
  };
  const auto variants = {minera::XesImportVariant::kIterparse,
                         minera::XesImportVariant::kIterparse20,
                         minera::XesImportVariant::kIterparseMemoryCompressed};
  for (const auto& [xml, message] : rejected) {
    for (const auto variant : variants) {
      minera::XesImportOptions options;
      options.variant = variant;
      bool matched = false;
      try {
        std::istringstream input(xml);
        (void)minera::import_xes_event_log(input, options);
      } catch (const std::exception& error) {
        matched = std::string(error.what()).find(message) != std::string::npos;
      }
      assert(matched);
    }
  }
  // A trailing orphan <event> and other processing instructions are read.
  const std::vector<std::string> accepted = {
      head + "<log>" + trace + event + "</log>",
      head + "<?xml-stylesheet href=\"a.xsl\"?>\n<log>" + trace + "</log>",
      "<?xmlfoo bar?>\n<log>" + trace + "</log>",
  };
  for (const auto& xml : accepted) {
    for (const auto variant : variants) {
      minera::XesImportOptions options;
      options.variant = variant;
      std::istringstream input(xml);
      const auto log = minera::import_xes_event_log(input, options);
      assert(log.traces.size() == 1 && log.traces[0].events.size() == 1);
    }
  }
}

void iterparse_reference_accepts_and_normalizes() {
  // An internal DTD entity is expanded.
  const auto entity = read_iterparse(
      "<?xml version=\"1.0\"?>\n<!DOCTYPE log [<!ENTITY act \"Expanded\">]>\n"
      "<log><trace><string key=\"concept:name\" value=\"c1\"/><event>"
      "<string key=\"concept:name\" value=\"&act;\"/></event></trace></log>");
  assert(entity.traces[0].events[0].activity == "Expanded");
  // Literal TAB/CR/LF become spaces; character references keep the character.
  const auto ws = read_iterparse(
      "<log><trace><string key=\"concept:name\" value=\"c&#10;1\"/><event>"
      "<string key=\"concept:name\" value=\"a\tb\nc\r\nd  e\"/>"
      "<string key=\"refs\" value=\"A&#9;B&#13;C&#10;D\"/></event></trace></log>");
  assert(ws.traces[0].case_id == "c\n1");
  assert(ws.traces[0].events[0].activity == "a b c d  e");
  assert(*ws.traces[0].events[0].attribute("refs") == "A\tB\rC\nD");
  // The writer escapes them, so the values survive a round trip.
  const auto written = minera::serialize_xes(ws, {});
  assert(written.find("c&#10;1") != std::string::npos);
  assert(written.find("A&#9;B&#13;C&#10;D") != std::string::npos);
  const auto back = read_iterparse(written);
  assert(back.traces[0].case_id == "c\n1");
  assert(*back.traces[0].events[0].attribute("refs") == "A\tB\rC\nD");
  // Keyless and valueless attributes: skipped, None for a string, False for
  // a boolean (the reference fails on a valueless number or date).
  const auto keyless = read_iterparse(one_event(
      "<string value=\"nokey\"/><string key=\"s\"/><id key=\"x\"/><boolean key=\"b\"/>"
      "<int key=\"i\"/><float key=\"f\"/><date key=\"d\"/>"));
  const auto& event = keyless.traces[0].events[0];
  assert(event.attributes.size() == 3);
  assert(find_attribute(event.attributes, "s")->is_null);
  assert(find_attribute(event.attributes, "x")->is_null);
  assert(!std::get<bool>(find_attribute(event.attributes, "b")->typed_value));
  // Containers are dropped at every level.
  const auto containers = read_iterparse(
      "<log><container key=\"lc\"/><trace><string key=\"concept:name\" value=\"c\"/>"
      "<container key=\"tc\"><string key=\"x\" value=\"1\"/></container><event>"
      "<string key=\"concept:name\" value=\"A\"/><container key=\"ec\"/></event></trace></log>");
  assert(containers.attributes.empty() && containers.traces[0].attributes.empty() &&
         containers.traces[0].events[0].attributes.empty());
  // Extensions and classifiers are keyed by name (last wins, first place);
  // incomplete ones are skipped.  A repeated global scope keeps the last.
  const auto metadata = read_iterparse(
      "<log><extension name=\"A\" prefix=\"a\" uri=\"u1\"/><extension name=\"B\" prefix=\"b\"/>"
      "<extension prefix=\"c\" uri=\"u3\"/><extension name=\"A\" prefix=\"a2\" uri=\"u4\"/>"
      "<global scope=\"event\"><string key=\"x\" value=\"1\"/></global>"
      "<global scope=\"event\"><string key=\"y\" value=\"2\"/></global>"
      "<global scope=\"log\"><string key=\"w\" value=\"4\"/></global>"
      "<classifier name=\"Q\" keys=\"concept:name\"/><classifier name=\"Q\" keys=\"org:resource\"/>"
      "<classifier name=\"K\"/><trace/></log>");
  assert(metadata.metadata.extensions.size() == 1);
  assert(metadata.metadata.extensions[0].prefix == "a2");
  assert(metadata.metadata.classifiers.size() == 1);
  assert(metadata.metadata.classifiers[0].keys == std::vector<std::string>{"org:resource"});
  assert(metadata.metadata.event_globals.size() == 1 &&
         metadata.metadata.event_globals[0].key == "y");
  assert(metadata.metadata.globals.size() == 2 && metadata.metadata.globals[1].scope == "log");
}

void iterparse_reference_column_order() {
  // pm4py to_data_frame on this log (stable across hash seeds).
  const auto log = read_iterparse(
      "<log xes.version=\"1.0\"><trace><int key=\"prio\" value=\"1\"/>"
      "<string key=\"concept:name\" value=\"c1\"/>"
      "<container key=\"tc\"><string key=\"x\" value=\"1\"/></container><event>"
      "<string key=\"org:resource\" value=\"R1\"/>"
      "<date key=\"time:timestamp\" value=\"2020-01-01T10:00:00+02:00\"/>"
      "<string key=\"cost\" value=\"3\"/><string key=\"concept:name\" value=\"A\"/>"
      "<string key=\"lifecycle:transition\" value=\"complete\"/></event><event>"
      "<string key=\"zeta\" value=\"z\"/><string key=\"concept:name\" value=\"B\"/>"
      "<date key=\"time:timestamp\" value=\"2020-01-01T11:00:00+02:00\"/></event></trace>"
      "<trace><string key=\"concept:name\" value=\"c2\"/><string key=\"newcase\" value=\"n\"/>"
      "<event><string key=\"alpha\" value=\"a\"/><string key=\"concept:name\" value=\"C\"/>"
      "<date key=\"time:timestamp\" value=\"2020-01-01T12:00:00+02:00\"/></event></trace></log>");
  const auto table = minera::event_log_to_table(log);
  const std::vector<std::string> expected = {
      "org:resource", "time:timestamp", "cost", "concept:name", "lifecycle:transition",
      "case:prio", "case:concept:name", "zeta", "alpha", "case:newcase"};
  assert(table.columns == expected);
}

void wide_elements_import_in_linear_time() {
  // 200k distinct attributes on one event took 88 s with a linear lookup.
  std::string xml = "<log><trace><string key=\"concept:name\" value=\"c\"/><event>"
                    "<string key=\"concept:name\" value=\"A\"/>";
  constexpr int kAttributes = 200000;
  for (int i = 0; i < kAttributes; ++i)
    xml += "<string key=\"k" + std::to_string(i) + "\" value=\"v\"/>";
  xml += "<string key=\"k7\" value=\"last\"/></event></trace></log>";
  const auto log = read_iterparse(xml);
  const auto& event = log.traces[0].events[0];
  assert(event.attributes.size() == static_cast<std::size_t>(kAttributes));
  assert(*event.attribute("k7") == "last");
  assert(minera::event_log_to_table(log).columns.size() ==
         static_cast<std::size_t>(kAttributes) + 2);
}

// Reference capture (pm4py 2.7.22, fresh processes): the ETREE writer raises
// "All strings must be XML compatible: Unicode or ASCII, no NULL bytes or
// control characters" for NUL, U+0001 and U+FFFE, and both writers raise
// UnicodeEncodeError for a lone surrogate. Its LINE_BY_LINE writer emits NUL,
// U+0001 and U+FFFE raw, and read_xes then fails on the file it wrote
// ("invalid character in attribute value"). Minera's writers refuse all of
// them (declared for LINE_BY_LINE), so every document they produce reads back.
void writer_refuses_text_xml_cannot_carry() {
  const std::vector<std::pair<std::string, std::string>> refused = {
      {std::string("a\0b", 3), "character U+0000 is not allowed in XML"},
      {"a\x01" "b", "character U+0001 is not allowed in XML"},
      {"a\xef\xbf\xbe" "b", "character U+FFFE is not allowed in XML"},
      {"a\xed\xa0\x80" "b", "invalid UTF-8 byte sequence"},  // WTF-8 lone surrogate
      {"a\xff" "b", "invalid UTF-8 byte sequence"},
      {"a\xc3", "invalid UTF-8 byte sequence"},
  };
  for (const auto variant : {minera::XesExportVariant::LINE_BY_LINE,
                             minera::XesExportVariant::ETREE}) {
    minera::XesExportOptions options;
    options.variant = variant;
    for (const auto& [text, reason] : refused) {
      for (int place = 0; place < 3; ++place) {
        auto log = sample_log();
        auto& event = log.traces[0].events[0];
        if (place == 0) event.attributes.push_back(minera::EventAttribute{"payload", text});
        else if (place == 1) event.attributes.push_back(minera::EventAttribute{text, "value"});
        else log.attributes.push_back(minera::EventAttribute{"opaque", text});
        bool threw = false;
        try {
          (void)minera::serialize_xes(log, options);
        } catch (const std::invalid_argument& error) {
          threw = std::string(error.what()) == "XES cannot represent this text: " + reason;
        }
        assert(threw);
      }
    }
    // Text XML can carry is written and read back unchanged.
    auto log = sample_log();
    const std::string text = "\xc3\xbf \xe2\x98\xba \xf0\x90\x80\x80 \xef\xbf\xbd \t";
    log.traces[0].events[0].attributes.push_back(minera::EventAttribute{"payload", text});
    std::istringstream input(minera::serialize_xes(log, options));
    const auto restored = minera::import_xes_event_log(input);
    assert(*restored.traces[0].events[0].attribute("payload") == text);
  }
}

int main() {
  unsigned_integer_boundaries_match_public_variants();
  {
    using namespace minera;
    const std::string xml = "<log><trace><string key=\"concept:name\" value=\"c\"/>"
        "<event><string key=\"concept:name\" value=\"A\"/><string key=\"group\" value=\"X\"/></event>"
        "<event><string key=\"concept:name\" value=\"B\"/><string key=\"group\" value=\"Y\"/></event>"
        "<event><string key=\"concept:name\" value=\"C\"/><string key=\"group\" value=\"X\"/></event></trace></log>";
    for (const bool gzip : {false, true}) {
      const auto read = [&](XesImportOptions options) {
        std::istringstream input(gzip ? detail::gzip_compress(xml) : xml);
        return import_xes_result(input, options);
      };
      XesImportOptions options;
      options.variant = XesImportVariant::RUSTXES;
      options.conversion_case_id_key = "group";
      options.conversion_extensions = std::vector<LogExtension>{};
      assert(read(options).log.metadata.extensions.empty());
      options.conversion_extensions = std::vector<LogExtension>{{"Custom", "custom", "urn:custom"}};
      assert(read(options).log.metadata.extensions[0].uri == "urn:custom");
      options.conversion_extensions = std::vector<LogExtension>{{"Custom", "custom", "urn:first"},
          {"Other", "custom", "urn:other"}, {"Custom", "last", "urn:last"}};
      const auto extensions = read(options).log.metadata.extensions;
      assert(extensions.size() == 2 && extensions[0].name == "Custom");
      assert(extensions[0].uri == "urn:last" && extensions[1].name == "Other");

      options.conversion_extensions.reset();
      const auto grouped = read(options).log;
      assert(grouped.traces.size() == 2 && grouped.traces[0].events.size() == 2);
      assert(grouped.traces[0].events[1].activity == "C");
      assert(grouped.traces[0].events[1].input_order == 2);
      assert(grouped.traces[0].case_id == "c" && grouped.traces[1].case_id == "c");
      assert(grouped.traces[0].events[0].attribute("group") && *grouped.traces[0].events[0].attribute("group") == "X");
      options.conversion_case_attribute_prefix = "x:";
      const auto retained = read(options).log;
      assert(retained.traces[0].case_id == "X");
      assert(retained.traces[0].events[0].attribute("case:concept:name"));
      options.conversion_case_attribute_prefix = "";
      const auto swallowed = read(options).log;
      assert(swallowed.traces[0].case_id == "A");
      assert(swallowed.traces[0].events[0].activity.empty());
      assert(swallowed.traces[0].events[0].attributes.empty());
      options.conversion_case_id_key.reset();
      assert(throws([&] { (void)read(options); }));
      options.return_legacy_log_object = false;
      assert(read(options).kind == EventDataKind::kEventTable);
      options.return_legacy_log_object = true;
      options.conversion_case_id_key = "group";
      options.conversion_case_attribute_prefix.reset();
      assert(throws([&] { (void)read(options); }));
      options.conversion_case_attribute_prefix = "case:";
      options.conversion_case_id_key = "date-group";
      const std::string dates = "<log><trace><event><date key=\"date-group\" value=\"2024-01-01T00:00:00Z\"/></event>"
          "<event><date key=\"date-group\" value=\"2024-01-01T00:00:00.000000Z\"/></event></trace></log>";
      std::istringstream date_input(gzip ? detail::gzip_compress(dates) : dates);
      const auto date_log = import_xes_result(date_input, options).log;
      assert(date_log.traces.size() == 1 && date_log.traces[0].events.size() == 2);
      options.case_attribute = "case:custom";
      options.activity_attribute = "";
      options.conversion_case_id_key = "group";
      const std::string custom = "<log><trace><string key=\"case:custom\" value=\"selected\"/>"
          "<event><string key=\"group\" value=\"X\"/><string key=\"\" value=\"empty-activity\"/></event></trace></log>";
      std::istringstream custom_input(gzip ? detail::gzip_compress(custom) : custom);
      const auto custom_log = import_xes_result(custom_input, options).log;
      assert(custom_log.traces[0].case_id == "selected");
      assert(custom_log.traces[0].case_id_attribute->key == "custom");
      assert(custom_log.traces[0].events[0].activity == "empty-activity");
      assert(custom_log.traces[0].events[0].activity_source_key.starts_with("column_"));
      assert(custom_log.traces[0].events[0].case_id == "selected");


    }
  }

  {
    using namespace minera;
    for (const bool gzip : {false, true}) for (const bool lazy : {false, true}) {
      const std::string xml = "<log><trace><string key=\"unused\" value=\"schema\"/></trace>"
          "<trace><event><string key=\"concept:name\" value=\"A\"/><int key=\"n\" value=\"1\"/>"
          "<string key=\"case:extra\" value=\"discard\"/></event></trace>"
          "<trace><event><string key=\"concept:name\" value=\"B\"/><int key=\"n\"/></event></trace></log>";
      std::istringstream input(gzip ? detail::gzip_compress(xml) : xml);
      XesImportOptions options;
      options.variant = XesImportVariant::RUSTXES;
      options.return_legacy_log_object = false;
      options.return_pl_lazyframe = lazy;
      const auto result = import_xes_result(input, options);
      assert(result.kind == (lazy ? EventDataKind::kLazyEventTable : EventDataKind::kEventTable));
      const auto table = lazy ? result.lazy_table.materialize() : result.table;
      assert(table.rows.size() == 2 && table.columns.size() == 4);
      assert(table.dtypes == result.dtypes);
      const auto n = table.column_index("n");
      assert(result.dtypes[n] == (lazy ? "Int64" : "float64"));
      assert(table.rows[0][table.column_index("concept:name")]->value == "A");
      assert(table.rows[1][table.column_index("concept:name")]->value == "B");
      assert(!table.rows[0][table.column_index("case:unused")]);
      assert(!table.rows[0][table.column_index("case:extra")]);
      if (lazy) {
        assert(!table.rows[1][n]);
        assert(table.rows[0][n]->kind == AttributeKind::kInt);
        assert(result.lazy_table.materialize().rows.size() == 2);
      } else {
        assert(table.rows[0][n]->kind == AttributeKind::kFloat);
        assert(std::isnan(std::get<double>(table.rows[1][n]->typed_value)));
      }
      std::istringstream legacy("<log><trace><string key=\"concept:name\" value=\"c\"/><event></event></trace></log>");
      options.return_legacy_log_object = true;
      options.return_pl_lazyframe = true;
      assert(import_xes_result(legacy, options).kind == EventDataKind::kEventLog);
      options.return_legacy_log_object = false;
      std::istringstream convenience("<log><trace><event></event></trace></log>");
      assert(throws([&] { (void)import_xes_event_log(convenience, options); }));
      options.variant = XesImportVariant::ITERPARSE;
      std::istringstream other("<log><trace><event></event></trace></log>");
      assert(import_xes_result(other, options).kind == EventDataKind::kEventLog);

    }
  }

  {
    using namespace minera;
    for (const bool compressed : {false, true}) {
      const auto read = [&](const std::string& body, const std::string& case_key = "concept:name") {
        const auto xml = "<log>" + body + "</log>";
        std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
        XesImportOptions options;
        options.variant = XesImportVariant::RUSTXES;
        options.case_attribute = case_key;
        return import_xes_event_log(input, options);
      };
      const auto trace = [](const std::string& id, const std::string& activity, const std::string& extra = "") {
        return "<trace><string key=\"concept:name\" value=\"" + id + "\"/>" + extra +
            "<event><string key=\"concept:name\" value=\"" + activity + "\"/><string key=\"case:probe\" value=\"discard\"/></event></trace>";
      };
      const auto grouped = read(trace("b", "A") + trace("a", "B") + trace("b", "C", "<string key=\"late\" value=\"ignored\"/>"));
      assert(grouped.traces.size() == 2);
      assert(grouped.traces[0].case_id == "b" && grouped.traces[1].case_id == "a");
      assert(grouped.traces[0].events.size() == 2);
      assert(grouped.traces[0].events[1].activity == "C");
      assert(!find_attribute(grouped.traces[0].attributes, "late"));
      assert(grouped.traces[0].events[0].attributes.empty());
      assert(throws([&] { (void)read("<trace><event></event></trace>"); }));
      assert(throws([&] { (void)read("<trace><string key=\"case:concept:name\" value=\"alias\"/><event></event></trace>"); }));
      const auto prefixed = read(trace("c", "A", "<string key=\"showcase:probe\" value=\"v\"/>"));
      assert(find_attribute(prefixed.traces[0].attributes, "showprobe")->value == "v");
      const auto extension = read("<trace><string key=\"case:org:resource\" value=\"r\"/></trace>");
      assert(extension.traces.empty());
      assert(extension.metadata.extensions.size() == 1);
      assert(extension.metadata.extensions[0].name == "Organizational");
      for (int selector = 0; selector < 5; ++selector) {
        XesImportOptions options;
        options.variant = XesImportVariant::RUSTXES;
        auto* key = selector == 0 ? &options.case_attribute : selector == 1 ? &options.activity_attribute :
            selector == 2 ? &options.timestamp_attribute : selector == 3 ? &options.resource_attribute : &options.lifecycle_attribute;
        key->clear();
        const std::string xml = "<log><trace><string key=\"concept:name\" value=\"c\"/><string key=\"\" value=\"trace-empty\"/>"
            "<event><string key=\"concept:name\" value=\"A\"/><string key=\"\" value=\"event-empty\"/></event></trace></log>";
        std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
        const auto selected_log = import_xes_event_log(input, options);
        const auto& selected_trace = selected_log.traces[0];
        const auto& event = selected_trace.events[0];
        const auto& metadata = selector == 0 ? selected_trace.case_id_attribute : selector == 1 ? event.activity_attribute :
            selector == 2 ? event.timestamp_attribute : selector == 3 ? event.resource_attribute : event.lifecycle_attribute;
        const auto state = selector == 0 ? selected_trace.case_id_state : selector == 1 ? event.activity_state :
            selector == 2 ? event.timestamp_state : selector == 3 ? event.resource_state : event.lifecycle_state;
        assert(metadata);
        assert(selector == 0 ? metadata->key.empty() : metadata->key.starts_with("column_"));
        assert(metadata->value == (selector == 0 ? "trace-empty" : "event-empty"));
        assert(state == FirstClassAttributeState::kPresent);
        assert(event.case_id == selected_trace.case_id);
        if (selector == 0) assert(selected_trace.case_id == "trace-empty");
        if (selector == 1) assert(event.activity == "event-empty");
        if (selector == 2) assert(event.timestamp == "event-empty");
        if (selector == 3) assert(event.resource == "event-empty");
        if (selector == 4) assert(event.lifecycle == "event-empty");
      }
      for (int sample = 0; sample < 24; ++sample) {
        const std::string body = "<trace><string key=\"concept:name\" value=\"c\"/><event>"
            "<string key=\"concept:name\" value=\"A\"/><string key=\"\" value=\"empty\"/>"
            "<string key=\"column_0\" value=\"explicit\"/></event></trace>";
        try {
          const auto collision = read(body);
          const auto& attributes = collision.traces[0].events[0].attributes;
          assert(find_attribute(attributes, "column_0")->value == "explicit");
          const auto generated = std::find_if(attributes.begin(), attributes.end(), [](const auto& value) {
            return value.value == "empty";
          });
          assert(generated != attributes.end());
          assert(generated->key == "column_1" || generated->key == "column_2" || generated->key == "column_3");
        } catch (const std::invalid_argument& error) {
          assert(std::string(error.what()).find("duplicate column column_0") != std::string::npos);
        }
      }
      const auto missing_key = read("<trace><string key=\"concept:name\" value=\"c\"/><string value=\"trace-no-key\"/>"
          "<event><string key=\"concept:name\" value=\"A\"/><string value=\"event-no-key\"/></event></trace>");
      assert(find_attribute(missing_key.traces[0].attributes, "")->value == "trace-no-key");
      assert(missing_key.traces[0].events[0].attributes.size() == 1);
      assert(missing_key.traces[0].events[0].attributes[0].key.starts_with("column_"));
      assert(missing_key.traces[0].events[0].attributes[0].value == "event-no-key");
      for (const auto& [source, expected] : std::vector<std::pair<std::string, std::string>>{
          {"2016-12-31T23:59:60Z", "2017-01-01T00:00:00.000Z"},
          {"2024-01-01T23:59:60.123456Z", "2024-01-02T00:00:00.123456Z"},
          {"2024-01-01T23:59:60.123456+05:30", "2024-01-01T18:30:00.123456Z"}}) {
        const auto leap = read("<trace><string key=\"concept:name\" value=\"c\"/><event><date key=\"time:timestamp\" value=\"" + source + "\"/></event></trace>");
        assert(leap.traces[0].events[0].timestamp == expected);
        assert(std::get<NativeDate>(leap.traces[0].events[0].timestamp_attribute->typed_value).value == expected);
      }
      assert(throws([&] { (void)read("<trace><string key=\"concept:name\" value=\"c\"/><event/></trace>"); }));
      assert(throws([&] { (void)read("<trace/>"); }));
      assert(throws([&] { (void)read("<trace/>" + trace("c", "A")); }));
      assert(read("<trace></trace><trace/>").traces.empty());
      assert(read(trace("c", "A") + "<trace/><trace/>").traces.size() == 1);
      const auto reserved_extension = read("<trace><string key=\"concept:name\" value=\"c\"/><event><string key=\"case:org:resource\" value=\"r\"/></event></trace>");
      assert(std::any_of(reserved_extension.metadata.extensions.begin(), reserved_extension.metadata.extensions.end(),
          [](const auto& extension) { return extension.name == "Organizational"; }));
      const auto missing_custom = read(trace("c", "A", "<string key=\"\" value=\"unrelated\"/>"), "missing");
      assert(!missing_custom.traces[0].case_id_attribute);
      assert(missing_custom.traces[0].case_id.empty());
      assert(find_attribute(missing_custom.traces[0].attributes, "")->value == "unrelated");
      assert(missing_custom.traces[0].events[0].case_id.empty());
      const auto custom = read(trace("c", "A", "<string key=\"case:custom\" value=\"selected\"/>"), "case:custom");
      assert(custom.traces[0].case_id == "selected");
      assert(custom.traces[0].case_id_source_key == "custom");
      assert(custom.traces[0].case_id_attribute->key == "custom");
      assert(!find_attribute(custom.traces[0].attributes, "custom"));
      assert(custom.traces[0].events[0].case_id == "selected");
      for (int sample = 0; sample < 24; ++sample) {
        const auto custom_collision = read(trace("c", "A", "<string key=\"case:custom\" value=\"selected\"/><string key=\"custom\" value=\"other\"/>"), "case:custom");
        const auto& selected = custom_collision.traces[0];
        assert(selected.case_id == "selected" || selected.case_id == "other");
        assert(selected.case_id_source_key == "custom" && selected.case_id_attribute->key == "custom");
        assert(selected.case_id_attribute->value == selected.case_id);
        assert(!find_attribute(selected.attributes, "custom"));
        assert(selected.events[0].case_id == selected.case_id);

        const auto collision = read(trace("c", "A", "<string key=\"case:concept:name\" value=\"alias\"/>") +
                                    trace("d", "B", "<string key=\"case:concept:name\" value=\"alias\"/>"));
        assert(collision.traces.size() == 2);
        assert((collision.traces[0].case_id == "c" && collision.traces[1].case_id == "d") ||
               (collision.traces[0].case_id == "alias" && collision.traces[1].case_id == "alias"));
        assert(collision.traces[0].events[0].case_id == collision.traces[0].case_id);
      }
    }
  }
  {
    using namespace minera;
    for (const bool compressed : {false, true}) {
      const auto read = [&](const std::string& attributes, const std::string& second) {
        const auto xml = "<log><trace><string key=\"concept:name\" value=\"c\"/><event>" + attributes + "</event><event>" + second + "</event></trace></log>";
        std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
        XesImportOptions options;
        options.variant = XesImportVariant::RUSTXES;
        return import_xes_event_log(input, options);
      };
      const auto nullable = read("<int key=\"probe\" value=\"42\"/>", "");
      for (const auto& [lexical, expected] : std::vector<std::pair<std::string, std::string>>{
          {"1.2345678901234567", "1.234568"}, {"1e20", "1.0000e20"},
          {"1e-10", "1.0000e-10"}, {"1e-7", "0.0000001"},
          {"-1e-7", "-1.0000e-7"}, {"1e6", "1e6"},
          {"1.0000001", "1.0000001"}, {"-1.0000001", "-1.0"},
          {"999999.123456789", "1.0000e6"}, {"999998.9999999", "999999.0"}}) {
        const auto display = read("<float key=\"probe\" value=\"" + lexical + "\"/>",
                                  "<string key=\"probe\" value=\"s\"/>");
        assert(find_attribute(display.traces[0].events[0].attributes, "probe")->value == expected);
      }
      const auto* promoted = find_attribute(nullable.traces[0].events[0].attributes, "probe");
      assert(promoted && promoted->kind == AttributeKind::kFloat);
      assert(std::get<double>(promoted->typed_value) == 42.0);
      const auto mixed = read("<int key=\"concept:name\" value=\"42\"/><float key=\"probe\" value=\"NaN\"/>",
          "<string key=\"concept:name\" value=\"A\"/><string key=\"probe\" value=\"text\"/>");
      assert(mixed.traces[0].events[0].activity == "42");
      assert(mixed.traces[0].events[1].activity == "\"A\"");
      assert(find_attribute(mixed.traces[0].events[0].attributes, "probe")->value == "NaN");
      const auto missing = read("<int key=\"probe\"/><int key=\"probe\" value=\"42\"/>",
          "<float key=\"probe\" value=\"NaN\"/>");
      assert(missing.traces[0].events[0].attributes.empty());
      assert(missing.traces[0].events[1].attributes.empty());
      const auto fallback = read("<int key=\"i\" value=\"bad\"/><float key=\"f\" value=\"1e999\"/><date key=\"d\" value=\"bad\"/><id key=\"id\" value=\"bad\"/>", "");
      const auto& attrs = fallback.traces[0].events[0].attributes;
      assert(std::get<double>(find_attribute(attrs, "i")->typed_value) == 0);
      assert(std::isinf(std::get<double>(find_attribute(attrs, "f")->typed_value)));
      assert(!find_attribute(attrs, "d"));
      assert(find_attribute(attrs, "id")->kind == AttributeKind::kString);
      for (const auto& [lexical, infinite] : std::vector<std::pair<std::string, bool>>{
          {"+1e999", true}, {"0." + std::string(400, '0') + "1", false},
          {std::string(400, '9') + "e-1", true}}) {
        const auto ranged = read("<float key=\"probe\" value=\"" + lexical + "\"/>", "");
        const auto number = std::get<double>(find_attribute(ranged.traces[0].events[0].attributes, "probe")->typed_value);
        assert(infinite ? std::isinf(number) : number == 0.0);
      }
      const std::string shared_xml = "<log><trace><string key=\"concept:name\" value=\"c\"/><event><int key=\"x\" value=\"42\"/></event><event></event></trace></log>";
      std::istringstream shared_input(compressed ? detail::gzip_compress(shared_xml) : shared_xml);
      XesImportOptions shared_options;
      shared_options.variant = XesImportVariant::RUSTXES;
      shared_options.activity_attribute = shared_options.resource_attribute = "x";
      const auto shared = import_xes_event_log(shared_input, shared_options);
      const auto& shared_event = shared.traces[0].events[0];
      assert(shared_event.activity_attribute->kind == AttributeKind::kFloat);
      assert(shared_event.resource_attribute->kind == AttributeKind::kFloat);
      assert(shared_event.activity == "42.0" && shared_event.resource == "42.0");
    }
  }
  {
    using namespace minera;
    const std::vector<std::pair<std::string, std::string>> shapes{
      {"", "[]"},
      {"<date key=\"d\" value=\"2024-01-01\"/><date key=\"leap\" value=\"2016-12-31t23:59:60z\"/><id key=\"id\" value=\"urn:uuid:ABCDEF00-1234-5678-1234-567812345678\"/>",
       "[Attribute { key: \"d\", value: None, own_attributes: None }, Attribute { key: \"leap\", value: Date(2016-12-31T23:59:60+00:00), own_attributes: None }, Attribute { key: \"id\", value: ID(abcdef00-1234-5678-1234-567812345678), own_attributes: None }]"},
      {"<int key=\"i\" value=\"bad\"/><float key=\"f\" value=\"bad\"/><date key=\"d\" value=\"bad\"/><id key=\"id\" value=\"bad\"/>",
       "[Attribute { key: \"i\", value: Int(0), own_attributes: None }, Attribute { key: \"f\", value: Float(0.0), own_attributes: None }, Attribute { key: \"d\", value: None, own_attributes: None }, Attribute { key: \"id\", value: ID(00000000-0000-0000-0000-000000000000), own_attributes: None }]"},
      {"<string key=\"x\" value=\"a\"/><string key=\"x\" value=\"b\"/>",
       "[Attribute { key: \"x\", value: String(\"a\"), own_attributes: None }, Attribute { key: \"x\", value: String(\"b\"), own_attributes: None }]"},
      {"<string key=\"x\" value=\"a\"><int key=\"n\" value=\"2\"/></string>",
       "[Attribute { key: \"x\", value: String(\"a\"), own_attributes: Some([Attribute { key: \"n\", value: Int(2), own_attributes: None }]) }]"},
      {"<float key=\"f\" value=\"1e16\"/><date key=\"d\" value=\"2024-01-01T00:00:00.1Z\"/>",
       "[Attribute { key: \"f\", value: Float(1e16), own_attributes: None }, Attribute { key: \"d\", value: Date(2024-01-01T00:00:00.100+00:00), own_attributes: None }]"},
      {"<int key=\"n\"/><id key=\"id\" value=\"ABCDEF00123456781234567812345678\"/>",
       "[Attribute { key: \"n\", value: None, own_attributes: None }, Attribute { key: \"id\", value: ID(abcdef00-1234-5678-1234-567812345678), own_attributes: None }]"},
      {"<string key=\"x\" value=\"&#160;&#769;&#10;\"/>",
       "[Attribute { key: \"x\", value: String(\"\\u{a0}\\u{301}\\n\"), own_attributes: None }]"}
    };
    for (const auto& [children, expected] : shapes) {
      for (const std::string container : {"list", "container"}) {
        for (const bool wrapped : {false, true}) {
          const auto attribute = "<" + container + " key=\"probe\">" +
              (wrapped ? "<values>" : "") + children +
              (wrapped ? "</values>" : "") + "</" + container + ">";
          const auto xml = "<log><trace><string key=\"concept:name\" value=\"c\"/>" + attribute +
              "<event><string key=\"concept:name\" value=\"A\"/>" + attribute + "</event></trace></log>";
          for (const bool compressed : {false, true}) {
            std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
            XesImportOptions options;
            options.variant = XesImportVariant::RUSTXES;
            const auto log = import_xes_event_log(input, options);
            const auto* trace_value = find_attribute(log.traces[0].attributes, "probe");
            const auto* event_value = find_attribute(log.traces[0].events[0].attributes, "probe");
            assert(trace_value && event_value);
            assert(trace_value->value == expected && event_value->value == expected);
            assert(event_value->kind == AttributeKind::kString);
            assert(event_value->container == AttributeContainerKind::kScalar && event_value->children.empty());
          }
        }
      }
    }
  }
  {
    using namespace minera;
    for (const bool compressed : {false, true}) {
      const auto read = [&](const std::string& xml) {
        std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
        XesImportOptions options;
        options.variant = XesImportVariant::RUSTXES;
        return import_xes_event_log(input, options);
      };
      const auto empty = read("<log/>");
      assert(empty.traces.empty() && empty.metadata.extensions.empty());
      assert(empty.attributes == std::vector<EventAttribute>({{"origin", "csv"}}));
      const auto empty_trace = read("<log><trace><string key=\"concept:name\" value=\"c\"/></trace></log>");
      assert(empty_trace.traces.empty());
      assert(empty_trace.metadata.extensions.size() == 1);
      assert(empty_trace.metadata.extensions[0].name == "Concept");
      const auto projected = read("<log><string key=\"origin\" value=\"source\"/><extension name=\"Custom\" prefix=\"custom\" uri=\"urn:custom\"/><classifier name=\"Class\" keys=\"concept:name\"/><global scope=\"event\"><string key=\"default\" value=\"drop\"/></global><trace><string key=\"concept:name\" value=\"c\"/><event><string key=\"concept:name\" value=\"A\"/><string key=\"custom:probe\" value=\"keep\"/></event></trace></log>");
      assert(projected.attributes == std::vector<EventAttribute>({{"origin", "csv"}}));
      assert(projected.metadata.extensions.size() == 1);
      assert(projected.metadata.extensions[0].prefix == "concept");
      assert(projected.metadata.classifiers.empty() && projected.metadata.globals.empty());
      assert(projected.traces[0].events[0].activity == "A");
      assert(find_attribute(projected.traces[0].events[0].attributes, "custom:probe")->value == "keep");
      const auto artifact = read("<log><trace><string key=\"concept:name\" value=\"c\"/><event><string key=\"artifactlifecycle:state\" value=\"active\"/></event></trace></log>");
      assert(artifact.metadata.extensions.size() == 2);
      assert(artifact.metadata.extensions[1].name == "ArtifactLifecycle");
    }
  }
  {
    using namespace minera;
    for (int shape = 0; shape < 8; ++shape) {
      EventAttribute value{"probe", "true", AttributeKind::kBoolean, shape < 5};
      value.scalar_wrapper = shape != 0;
      value.scalar_children_list = shape == 2 || shape == 4 || shape == 6;
      if (shape >= 3) value.children = {{"child", "", AttributeKind::kString, true}, {"kept", "value"}};
      if (shape == 7) {
        value.kind = AttributeKind::kString;
        value.value = "parent";
        EventAttribute nested{"child", "", AttributeKind::kString, true};
        nested.scalar_wrapper = true;
        nested.children = {{"kept", "value"}};
        value.children = {nested};
      }
      for (int scope = 0; scope < 4; ++scope) {
        EventLog log;
        log.traces.push_back(Trace{"c", {Event{"c", "A"}}});
        if (scope == 0) log.attributes = {value};
        if (scope == 1) log.metadata.globals.push_back({"event", {value}});
        if (scope == 2) log.traces[0].attributes = {value};
        if (scope == 3) log.traces[0].events[0].attributes = {value};
        for (const auto variant : {XesExportVariant::ETREE, XesExportVariant::LINE_BY_LINE}) {
          for (const bool compressed : {false, true}) {
            XesExportOptions options;
            options.variant = variant;
            options.compress = compressed;
            if (shape == 6 && variant == XesExportVariant::LINE_BY_LINE) {
              assert(throws([&] { (void)serialize_xes(log, options); }));
              continue;
            }
            const auto bytes = serialize_xes(log, options);
            const auto xml = compressed ? detail::gzip_decompress(bytes, 1024 * 1024) : bytes;
            assert(xml.find("null=\"true\"") == std::string::npos);
            assert((xml.find("key=\"probe\"") != std::string::npos) == (shape != 0));
            if (shape > 0 && shape < 5) assert(xml.find("<list key=\"probe\">") != std::string::npos);
            if (shape >= 3 && !(shape == 3 && variant == XesExportVariant::LINE_BY_LINE) &&
                !(shape == 7 && variant == XesExportVariant::LINE_BY_LINE))
              assert(xml.find("key=\"kept\" value=\"value\"") != std::string::npos);
            if (shape == 3 && variant == XesExportVariant::LINE_BY_LINE)
              assert(xml.find("key=\"c\" value=\"h\"") != std::string::npos);
          }
        }
      }
    }
    EventLog selected;
    Trace trace;
    trace.case_id_state = FirstClassAttributeState::kNull;
    Event event;
    event.activity_state = event.timestamp_state = event.resource_state =
        event.lifecycle_state = FirstClassAttributeState::kNull;
    trace.events.push_back(event);
    selected.traces.push_back(trace);
    assert(to_xes(selected).find("null=\"true\"") == std::string::npos);
    assert(occurrences(serialize_xes(selected, {.preserve_nulls = true}), "null=\"true\"") == 5);
    for (const auto kind : {AttributeContainerKind::kTuple, AttributeContainerKind::kSet,
                            AttributeContainerKind::kFrozenSet}) {
      EventLog null_container;
      EventAttribute value{"probe", "stale", AttributeKind::kString, true};
      value.container = kind;
      value.children = {{"x", "invalid-date", AttributeKind::kDate}};
      null_container.attributes.push_back(value);
      for (const auto variant : {XesExportVariant::ETREE, XesExportVariant::LINE_BY_LINE}) {
        assert(serialize_xes(null_container, {.variant = variant}).find("key=\"probe\"") == std::string::npos);
        const auto preserved = serialize_xes(null_container, {.variant = variant, .preserve_nulls = true});
        assert(preserved.find("key=\"probe\" null=\"true\"") != std::string::npos);
        assert(preserved.find("invalid-date") == std::string::npos);
      }
    }
  }
  {
    using namespace minera;
    const std::string body = "<log>\n<trace>\n<event>\n";
    const std::string tail = "\n</event>\n</trace>\n</log>";
    for (const bool compressed : {false, true}) {
      const auto read = [&](const std::string& xml, XesImportOptions options = {}) {
        options.variant = XesImportVariant::LINE_BY_LINE;
        std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
        return import_xes_event_log(input, options);
      };
      assert(read("<log><trace><event/></trace></log>").traces.empty());
      assert(read("<log>\n<trace>\n<event/>\n</trace>\n</log>").traces.empty());
      assert(throws([&] { (void)read(body + "<int key=\"bad\" value=\"invalid\"/>"); }));
      assert(read("<log>\n<int key=\"bad\" value=\"invalid\"/>\n</log>").traces.empty());
      const auto packed = read(body + "<boolean key=\"parent\" value=\"true\"><string key=\"child\" value=\"v\"/></boolean>" + tail);
      assert(packed.traces.size() == 1 && packed.traces[0].events.size() == 1);
      assert(packed.traces[0].events[0].attributes.empty());
      const auto flat = read(body + "<boolean key=\"parent\" value=\"true\">\n<string key=\"child\" value=\"v\"/>\n</boolean>" + tail);
      assert(find_attribute(flat.traces[0].events[0].attributes, "parent"));
      assert(find_attribute(flat.traces[0].events[0].attributes, "child"));
      const auto lexical = read(body + "<string value=\"v\" key=\"k\"/>\n<string key=\"entity\" value=\"&amp;&#65;\"/>\n<string key='ignored' value='v'/>" + tail);
      assert(find_attribute(lexical.traces[0].events[0].attributes, "v")->value == "k");
      assert(find_attribute(lexical.traces[0].events[0].attributes, "entity")->value == "&amp;&#65;");
      assert(!find_attribute(lexical.traces[0].events[0].attributes, "ignored"));
      const auto kinds = read(body + "<boolean key=\"flag\" value=\"true\"></unknown>\n<unknown key=\"text\" value=\"true\"></boolean>" + tail);
      assert(find_attribute(kinds.traces[0].events[0].attributes, "flag")->kind == AttributeKind::kBoolean);
      assert(find_attribute(kinds.traces[0].events[0].attributes, "text")->kind == AttributeKind::kString);
      XesImportOptions selected;
      selected.activity_attribute = "custom";
      selected.attributes_to_read = {"keep"};
      const auto projected = read(body + "<string key=\"custom\" value=\"A\"/>\n<string key=\"drop\" value=\"x\"/>" + tail, selected);
      assert(projected.traces[0].events[0].activity == "A");
      assert(projected.traces[0].events[0].attributes.empty());
      selected.require_timestamp = true;
      assert(throws([&] { (void)read(body + tail, selected); }));
      XesImportOptions limited;
      limited.max_traces = 0;
      assert(read(body + tail, limited).traces.empty());
    }
  }
  {
    using namespace minera;
    const std::string children = "<string key=\"same\" value=\"first\"/><int key=\"same\" value=\"2\"/>";
    for (const auto variant : {XesImportVariant::kIterparse,
         XesImportVariant::kIterparseMemoryCompressed, XesImportVariant::kIterparse20}) {
      for (const bool compressed : {false, true}) {
        for (int shape = 0; shape < 4; ++shape) {
          const auto inner = shape == 0 ? "<values/>" : shape == 1 ?
              "<values>" + children + "</values><values>" + children + "</values>" :
              shape == 2 ? children + "<values/>" : "<values/>" + children;
          const auto xml = "<log><trace><event><boolean key=\"probe\" value=\"true\">" + inner + "</boolean></event></trace></log>";
          std::istringstream input(compressed ? detail::gzip_compress(xml) : xml);
          XesImportOptions options;
          options.variant = variant;
          const auto log = import_xes_event_log(input, options);
          const auto* probe = find_attribute(log.traces[0].events[0].attributes, "probe");
          assert(probe);
          const bool list = variant != XesImportVariant::kIterparse20 && shape != 2;
          assert(probe->scalar_children_list == list);
          assert(probe->children.size() == ((shape == 2 || shape == 3 ||
                  (shape == 1 && list)) ? 2 : 0));
        }
      }
    }
    EventAttribute pairs{"probe", "true", AttributeKind::kBoolean};
    pairs.scalar_children_list = true;
    for (const bool populated : {false, true}) {
      if (populated) pairs.children = {{"same", "first"}, {"same", "2", AttributeKind::kInt}};
      auto mapping = pairs;
      mapping.scalar_children_list = false;
      assert(mapping != pairs);
      for (int scope = 0; scope < 5; ++scope) {
        EventLog log;
        log.traces.push_back(Trace{"c", {Event{"c", "true"}}});
        if (scope == 0) log.attributes = {pairs};
        if (scope == 1) log.traces[0].attributes = {pairs};
        if (scope == 2) {
          EventAttribute outer{"outer", "v"}; outer.children = {pairs};
          log.traces[0].events[0].attributes = {outer};
        }
        if (scope == 3) {
          auto& event = log.traces[0].events[0];
          event.activity_attribute = pairs;
          event.activity_source_key = "probe";
        }
        if (scope == 4) log.metadata.globals.push_back({"event", {pairs}});
        for (const bool compressed : {false, true}) {
          XesExportOptions options;
          options.compress = compressed;
          options.variant = XesExportVariant::ETREE;
          const auto bytes = serialize_xes(log, options);
          const auto xml = compressed ? detail::gzip_decompress(bytes, 1024 * 1024) : bytes;
          assert(xml.find("key=\"probe\" value=\"True\"") != std::string::npos);
          assert(occurrences(xml, "key=\"same\"") == (populated ? 2 : 0));
          options.variant = XesExportVariant::LINE_BY_LINE;
          assert(throws([&] { (void)serialize_xes(log, options); }));
        }
      }
    }
    Trace scalarized;
    scalarized.case_id_attribute = pairs;
    set_trace_case_id(scalarized, "new");
    assert(!scalarized.case_id_attribute->scalar_children_list);
    Event source, target;
    source.activity_attribute = pairs;
    set_event_activity_context(source, "new", target);
    assert(!target.activity_attribute->scalar_children_list);
  }
  {
    using namespace minera;
    for (const auto kind : {AttributeKind::kBoolean, AttributeKind::kDate}) {
      EventAttribute wrapped{"wrapped", kind == AttributeKind::kBoolean ?
          "true" : "2024-01-01T00:00:00Z", kind};
      auto scalar = wrapped;
      wrapped.scalar_wrapper = true;
      assert(wrapped != scalar);
      assert(EventAttribute(wrapped) == wrapped);
      for (int scope = 0; scope < 11; ++scope) {
        EventLog log;
        log.traces.push_back(Trace{"c", {Event{"c", "A"}}});
        auto& trace = log.traces[0];
        auto& event = trace.events[0];
        if (scope == 0) log.attributes = {wrapped};
        if (scope == 1) log.metadata.globals.push_back({"event", {wrapped}});
        if (scope == 2) trace.attributes = {wrapped};
        if (scope == 3) event.attributes = {wrapped};
        if (scope == 4) {
          EventAttribute parent{"outer", "metadata"};
          parent.children = {wrapped};
          event.attributes = {parent};
        }
        if (scope == 5 || scope == 6) {
          event.activity = wrapped.value;
          event.activity_source_key = wrapped.key;
          event.activity_state = FirstClassAttributeState::kPresent;
          if (scope == 5) event.activity_attribute = wrapped;
          else event.attributes = {wrapped};
        }
        if (scope == 7) {
          event.timestamp = wrapped.value;
          event.timestamp_source_key = wrapped.key;
          event.timestamp_state = FirstClassAttributeState::kPresent;
          event.timestamp_attribute = wrapped;
        }
        if (scope == 8) {
          trace.case_id = wrapped.value;
          trace.case_id_source_key = wrapped.key;
          trace.case_id_state = FirstClassAttributeState::kPresent;
          trace.case_id_attribute = wrapped;
        }
        if (scope == 9) {
          event.resource = wrapped.value;
          event.resource_source_key = wrapped.key;
          event.resource_state = FirstClassAttributeState::kPresent;
          event.resource_attribute = wrapped;
        }
        if (scope == 10) {
          event.lifecycle = wrapped.value;
          event.lifecycle_source_key = wrapped.key;
          event.lifecycle_state = FirstClassAttributeState::kPresent;
          event.lifecycle_attribute = wrapped;
        }
        for (const auto variant : {XesExportVariant::ETREE, XesExportVariant::LINE_BY_LINE}) {
          for (const bool compressed : {false, true}) {
            XesExportOptions options;
            options.variant = variant;
            options.compress = compressed;
            const auto bytes = serialize_xes(log, options);
            const auto xml = compressed ? detail::gzip_decompress(bytes, 1024 * 1024) : bytes;
            const std::string expected = kind == AttributeKind::kBoolean ?
                (variant == XesExportVariant::ETREE ? "True" : "true") :
                (variant == XesExportVariant::ETREE ? "2024-01-01 00:00:00+00:00" : "2024-01-01T00:00:00+00:00");
            assert(occurrences(xml, "key=\"wrapped\" value=\"" + expected + "\"") == 1);
          }
        }
      }
      Trace trace;
      trace.case_id_attribute = wrapped;
      set_trace_case_id(trace, "derived");
      assert(!trace.case_id_attribute->scalar_wrapper);
      Event source, target;
      source.activity_attribute = wrapped;
      set_event_activity_context(source, "derived", target);
      assert(!target.activity_attribute->scalar_wrapper);
      source.timestamp_attribute = wrapped;
      copy_event_timestamp_context(source, target);
      assert(target.timestamp_attribute->scalar_wrapper);
    }
  }
  boolean_import_spelling_matches_public_variants();
  boolean_export_spelling_matches_variant_and_scalar_shape();
  nested_attribute_variations_match_public_categories();
  date_import_sorting_uses_exact_fields_and_variant_rules();
  date_import_variations_retain_wall_clock_fields();
  date_export_preserves_source_precision();
  nested_export_variants_match_public_mapping_shapes();
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
  compresses_gzip_exports();
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
  null_scalar_list_children_preserve_order_and_duplicates();
  typed_first_class_selectors_survive_flat_adapters();
  iterparse_reference_typed_values();
  iterparse_reference_dates();
  iterparse_reference_rejects_malformed_xml();
  iterparse_reference_rejects_misplaced_elements_and_declarations();
  writer_refuses_text_xml_cannot_carry();
  iterparse_reference_accepts_and_normalizes();
  iterparse_reference_column_order();
  wide_elements_import_in_linear_time();
  return 0;
}
