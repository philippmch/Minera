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

}  // namespace

int main() {
  variants_and_gzip_round_trip();
  imports_external_dynamic_huffman_gzip();
  imports_fixed_huffman_and_concatenated_gzip();
  rejects_corruption_and_expansion_beyond_limit();
  file_export_uses_gzip_suffix_and_imports_it();
  return 0;
}
