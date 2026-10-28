#include "minera/ocel.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace minera;

namespace {
using Edge = std::pair<std::string, std::string>;

std::string bytes(std::string_view hex) {
  const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  assert(hex.size() % 2 == 0);
  std::string value;
  for (std::size_t i = 0; i < hex.size(); i += 2)
    value.push_back(static_cast<char>(16 * digit(hex[i]) + digit(hex[i + 1])));
  return value;
}

struct CapturedCase {
  const char* name;
  unsigned mask;
  std::string change;
  std::vector<std::size_t> relation_order;
  std::vector<Edge> expected;
  std::optional<std::string> missing_event;
  std::optional<Ocel> explicit_model;
};

const std::vector<CapturedCase>& cases() {
  // Literal outcomes from 119 actual public codeath calls (95 returns,
  // 24 KeyErrors), not expectations calculated by the native algorithm.
  // Capture: build/ocel-lifetime-graphs-focused-public-actual-1/records.jsonl
  // SHA-256: 4a26aaea5b361fbd0b8999d7037f7af186c2d202a16a22380f99bdabad673f9f
  static const std::vector<CapturedCase> values = {
      {"incidence-0", 0, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-1", 1, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-2", 2, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-3", 3, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-4", 4, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-5", 5, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-6", 6, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-7", 7, "", {}, {}, bytes("6531"), std::nullopt},
      {"incidence-8", 8, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-9", 9, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-10", 10, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-11", 11, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-12", 12, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-13", 13, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-14", 14, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-15", 15, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-16", 16, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-17", 17, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-18", 18, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-19", 19, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-20", 20, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-21", 21, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-22", 22, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-23", 23, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-24", 24, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-25", 25, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-26", 26, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-27", 27, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-28", 28, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-29", 29, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-30", 30, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-31", 31, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-32", 32, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-33", 33, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-34", 34, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-35", 35, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-36", 36, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-37", 37, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-38", 38, "", {}, {}, std::nullopt, std::nullopt},
      {"incidence-39", 39, "", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"incidence-40", 40, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-41", 41, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-42", 42, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-43", 43, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-44", 44, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-45", 45, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-46", 46, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-47", 47, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-48", 48, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-49", 49, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-50", 50, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-51", 51, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-52", 52, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-53", 53, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-54", 54, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-55", 55, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-56", 56, "", {}, {}, bytes("6530"), std::nullopt},
      {"incidence-57", 57, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-58", 58, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-59", 59, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-60", 60, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-61", 61, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-62", 62, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"incidence-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"parameters-none-0", 0, "", {}, {}, bytes("6531"), std::nullopt},
      {"parameters-empty-0", 0, "", {}, {}, bytes("6531"), std::nullopt},
      {"parameters-irrelevant-0", 0, "", {}, {}, bytes("6531"), std::nullopt},
      {"parameters-none-7", 7, "", {}, {}, bytes("6531"), std::nullopt},
      {"parameters-empty-7", 7, "", {}, {}, bytes("6531"), std::nullopt},
      {"parameters-irrelevant-7", 7, "", {}, {}, bytes("6531"), std::nullopt},
      {"parameters-none-51", 51, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"parameters-empty-51", 51, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"parameters-irrelevant-51", 51, "", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"parameters-none-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"parameters-empty-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"parameters-irrelevant-63", 63, "", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"duplicate-identical", 63, "duplicate-identical", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"duplicate-qualified", 63, "duplicate-qualified", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"relation-reversed", 63, "relation-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"events-reversed", 63, "events-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"objects-reversed", 63, "objects-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"timestamps-reversed", 63, "timestamps-reversed", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"unicode-ids", 63, "unicode-ids", {}, {{bytes("20ceb120"), bytes("f09f9880")}, {bytes("20ceb120"), bytes("71756f746564220a")}, {bytes("71756f746564220a"), bytes("f09f9880")}}, std::nullopt, std::nullopt},
      {"empty-object-id", 63, "empty-object-id", {}, {{bytes(""), bytes("6f31")}, {bytes(""), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"empty-event-id", 63, "empty-event-id", {}, {{bytes("6f30"), bytes("6f31")}, {bytes("6f30"), bytes("6f32")}, {bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"unused-records", 63, "unused-records", {}, {}, bytes("756e72656c61746564"), std::nullopt},
      {"objects-only", 63, "objects-only", {}, {}, std::nullopt, std::nullopt},
      {"events-only", 63, "events-only", {}, {}, bytes("6531"), std::nullopt},
      {"empty", 63, "empty", {}, {}, std::nullopt, std::nullopt},
      {"custom-columns", 21, "", {}, {{bytes("6f30"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"order-relations-reversed", 51, "relation-reversed", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"order-events-reversed", 51, "events-reversed", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"order-timestamps-reversed", 51, "timestamps-reversed", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"order-relations-interleaved", 51, "", {0, 2, 1, 3}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"order-relation-leading-later-event", 51, "", {2, 0, 3, 1}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"order-events-and-relations-reversed", 51, "events-and-relations-reversed", {}, {{bytes("6f30"), bytes("6f31")}}, std::nullopt, std::nullopt},
      {"order-tied-timestamps", 51, "tied-timestamps", {}, {{bytes("6f31"), bytes("6f32")}}, std::nullopt, std::nullopt},
      {"lifetime-chain", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6533"), bytes("63"), bytes("")}}}},
      {"lifetime-split-merge", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}, {bytes("64"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6531"), bytes("64"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6532"), bytes("64"), bytes("")}}}},
      {"lifetime-ephemeral", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("61"), bytes("63")}, {bytes("62"), bytes("63")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6530"), bytes("63"), bytes("")}}}},
      {"lifetime-samebirth-differentdeath", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}}}},
      {"lifetime-differentbirth-samedeath", 63, "", {}, {{bytes("61"), bytes("62")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}}}},
      {"lifetime-separatebirth-jointdeath", 63, "", {}, {{bytes("61"), bytes("62")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("61"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}}}},
      {"lifetime-chain-reverse-events", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6533"), bytes("63"), bytes("")}}}},
      {"lifetime-chain-reverse-relations", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6533"), bytes("63"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6530"), bytes("61"), bytes("")}}}},
      {"lifetime-chain-reverse-timestamps", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6533"), bytes("63"), bytes("")}}}},
      {"lifetime-chain-interleave-relations", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6533"), bytes("63"), bytes("")}}}},
      {"lifetime-split-merge-reverse-events", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}, {bytes("64"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6531"), bytes("64"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6532"), bytes("64"), bytes("")}}}},
      {"lifetime-split-merge-reverse-relations", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}, {bytes("64"), bytes("54"), {}}}, {{bytes("6532"), bytes("64"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6531"), bytes("64"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6530"), bytes("61"), bytes("")}}}},
      {"lifetime-split-merge-reverse-timestamps", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}, {bytes("64"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6531"), bytes("64"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6532"), bytes("64"), bytes("")}}}},
      {"lifetime-split-merge-interleave-relations", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}, {bytes("64"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("64"), bytes("")}, {bytes("6532"), bytes("64"), bytes("")}}}},
      {"lifetime-chain-lexical-eventids", 63, "", {}, {}, std::nullopt, Ocel{{{bytes("7a39"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("7a38"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("7a37"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("7a36"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("7a39"), bytes("61"), bytes("")}, {bytes("7a38"), bytes("61"), bytes("")}, {bytes("7a38"), bytes("62"), bytes("")}, {bytes("7a37"), bytes("62"), bytes("")}, {bytes("7a37"), bytes("63"), bytes("")}, {bytes("7a36"), bytes("63"), bytes("")}}}},
      {"lifetime-ephemeral-empty-object", 63, "", {}, {{bytes(""), bytes("62")}, {bytes(""), bytes("63")}, {bytes("62"), bytes("63")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes(""), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes(""), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6530"), bytes("63"), bytes("")}}}},
      {"lifetime-ephemeral-empty-event", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("61"), bytes("63")}, {bytes("62"), bytes("63")}}, std::nullopt, Ocel{{{bytes(""), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes(""), bytes("61"), bytes("")}, {bytes(""), bytes("62"), bytes("")}, {bytes(""), bytes("63"), bytes("")}}}},
      {"lifetime-unrelated-middle-event", 63, "", {}, {}, bytes("756e72656c617465642d6d6964646c65"), Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("756e72656c617465642d6d6964646c65"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6533"), bytes("63"), bytes("")}}}},
      {"lifetime-independent-object-types", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("747970652d30"), {}}, {bytes("62"), bytes("747970652d31"), {}}, {bytes("63"), bytes("747970652d32"), {}}, {bytes("64"), bytes("747970652d33"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6531"), bytes("64"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6532"), bytes("64"), bytes("")}}}},
      {"lifetime-revisit", 63, "", {}, {{bytes("62"), bytes("63")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("63"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}}}},
      {"lifetime-cohort-replacement", 63, "", {}, {{bytes("61"), bytes("62")}, {bytes("63"), bytes("64")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}, {bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30335a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}, {bytes("63"), bytes("54"), {}}, {bytes("64"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("63"), bytes("")}, {bytes("6532"), bytes("64"), bytes("")}, {bytes("6533"), bytes("63"), bytes("")}, {bytes("6533"), bytes("64"), bytes("")}}}},
      {"lifetime-simultaneous-lifetime", 63, "", {}, {{bytes("61"), bytes("62")}}, std::nullopt, Ocel{{{bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30305a"), {}}, {bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30315a"), {}}, {bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30325a"), {}}}, {{bytes("61"), bytes("54"), {}}, {bytes("62"), bytes("54"), {}}}, {{bytes("6530"), bytes("61"), bytes("")}, {bytes("6530"), bytes("62"), bytes("")}, {bytes("6531"), bytes("61"), bytes("")}, {bytes("6531"), bytes("62"), bytes("")}, {bytes("6532"), bytes("61"), bytes("")}, {bytes("6532"), bytes("62"), bytes("")}}}},
  };
  return values;
}

Ocel model(const CapturedCase& spec) {
  if (spec.explicit_model) return *spec.explicit_model;
  std::vector<std::string> objects = {"o0", "o1", "o2"};
  std::vector<std::string> events = {"e0", "e1"};
  if (spec.change == "unicode-ids") {
    objects = {bytes("20ceb120"), bytes("f09f9880"), "quoted\"\n"};
    events = {" event,0 ", "event->1"};
  }
  if (spec.change == "empty-object-id") objects.front().clear();
  if (spec.change == "empty-event-id") events.front().clear();
  Ocel value;
  for (const auto& id : objects) value.objects.push_back({id, "T", {}});
  for (std::size_t i = 0; i < events.size(); ++i)
    value.events.push_back({events[i], "A", "2026-01-01T00:00:0" +
        std::to_string(i) + "Z", {}});
  for (unsigned e = 0; e < 2; ++e)
    for (unsigned o = 0; o < 3; ++o)
      if (spec.mask & (1u << (3 * e + o)))
        value.relations.push_back({events[e], objects[o], ""});
  if (spec.change.starts_with("duplicate")) {
    auto duplicate = value.relations.front();
    if (spec.change == "duplicate-qualified") duplicate.qualifier = "other role";
    value.relations.push_back(std::move(duplicate));
  }
  if (!spec.relation_order.empty()) {
    const auto original = value.relations;
    value.relations.clear();
    for (const auto index : spec.relation_order)
      value.relations.push_back(original.at(index));
  }
  if (spec.change == "relation-reversed")
    std::reverse(value.relations.begin(), value.relations.end());
  if (spec.change == "events-and-relations-reversed") {
    std::reverse(value.events.begin(), value.events.end());
    std::reverse(value.relations.begin(), value.relations.end());
  }
  if (spec.change == "tied-timestamps")
    for (auto& event : value.events) event.timestamp = "2026-01-01T00:00:00Z";
  if (spec.change == "events-reversed")
    std::reverse(value.events.begin(), value.events.end());
  if (spec.change == "objects-reversed")
    std::reverse(value.objects.begin(), value.objects.end());
  if (spec.change == "timestamps-reversed")
    for (std::size_t i = 0; i < value.events.size(); ++i)
      value.events[i].timestamp = "2026-01-01T00:00:0" + std::to_string(1-i) + "Z";
  if (spec.change == "unused-records") {
    value.objects.push_back({"isolated", "unused", {}});
    value.events.push_back({"unrelated", "unused", "2026-01-01T00:00:09Z", {}});
  }
  if (spec.change == "objects-only" || spec.change == "events-only" ||
      spec.change == "empty") {
    value.relations.clear();
    if (spec.change != "events-only") value.events.clear();
    if (spec.change != "objects-only") value.objects.clear();
  }
  return value;
}

void check(const Ocel& source, std::vector<Edge> expected,
           OcelEnrichmentGraph graph = OcelEnrichmentGraph::kCodeath,
           const std::optional<std::string>& missing_event = std::nullopt) {
  const auto before = serialize_oc_playout_workbench_ocel_json(source);
  try {
    const auto result = discover_ocel_object_graph(source, graph);
    if (missing_event)
      throw std::runtime_error("expected missing-event diagnostic for " + *missing_event);
    std::sort(expected.begin(), expected.end());
    assert(result.graph == OcelEnrichmentGraph::kCodeath);
    if (result.edges != expected) throw std::runtime_error("captured edge set differs");
    std::vector<std::string> ids;
    for (const auto& object : source.objects) ids.push_back(object.id);
    std::sort(ids.begin(), ids.end());
    assert(result.object_ids == ids);
  } catch (const std::out_of_range& error) {
    if (!missing_event) throw;
    assert(error.what() == "OCEL codeath event has no object relations: " + *missing_event);
  }
  assert(serialize_oc_playout_workbench_ocel_json(source) == before);
}

unsigned retained_public_codeath_cases() {
  unsigned failures = 0;
  for (const auto& spec : cases()) {
    try { check(model(spec), spec.expected, OcelEnrichmentGraph::kCodeath, spec.missing_event); }
    catch (const std::exception& error) {
      ++failures;
      std::cerr << spec.name << ": " << error.what() << '\n';
    }
  }
  std::cout << "Retained direct public codeath cases: "
            << cases().size() - failures << '/' << cases().size() << " passed\n";
  return failures;
}

void metadata_and_literal_identity_invariance() {
  auto source = model(cases().at(51));
  const auto expected = cases().at(51).expected;
  const std::string odd = bytes("2000ffeda08022");
  for (auto& event : source.events) {
    event.activity = odd;
    event.timestamp = "invalid timestamp is irrelevant to first event-table encounter";
    event.attributes = {{odd, nullptr}, {odd, true}, {odd, std::int64_t{-7}},
                        {odd, -0.0}, {odd, odd}};
  }
  for (auto& object : source.objects) {
    object.type = odd;
    object.attributes = {{odd, std::numeric_limits<double>::infinity()}};
  }
  for (auto& relation : source.relations) relation.qualifier = odd;
  source.o2o = {{"o0", "o2", odd}};
  source.e2e = {{"e0", "e1", odd}};
  source.object_changes = {{"o0", odd, odd, odd, odd}};
  source.globals[odd] = odd;
  source.event_type_attributes[odd][odd] = odd;
  source.object_type_attributes[odd][odd] = odd;
  source.table_columns.events = std::vector<std::string>{odd};
  source.table_columns.objects = std::vector<std::string>{};
  source.table_columns.relations = std::vector<std::string>{odd, odd};
  check(source, expected);
  // All ordinary string bytes remain identity data, including embedded NULs.
  assert(!expected.empty());
  const auto previous_id = expected.front().first;
  for (auto& object : source.objects)
    if (object.id == previous_id) object.id = odd;
  for (auto& relation : source.relations)
    if (relation.object_id == previous_id) relation.object_id = odd;
  for (auto& relation : source.o2o) {
    if (relation.source_id == previous_id) relation.source_id = odd;
    if (relation.target_id == previous_id) relation.target_id = odd;
  }
  for (auto& change : source.object_changes)
    if (change.object_id == previous_id) change.object_id = odd;
  std::vector<Edge> renamed;
  for (auto edge : expected) {
    if (edge.first == previous_id) edge.first = odd;
    if (edge.second == previous_id) edge.second = odd;
    if (edge.second < edge.first) std::swap(edge.first, edge.second);
    renamed.push_back(std::move(edge));
  }
  check(source, renamed);
}

void preserved_validation_and_other_selectors() {
  const auto rejects = [](const Ocel& source, OcelEnrichmentGraph graph) {
    const auto before = serialize_oc_playout_workbench_ocel_json(source);
    bool threw = false;
    try { (void)discover_ocel_object_graph(source, graph); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    assert(serialize_oc_playout_workbench_ocel_json(source) == before);
  };
  auto source = model(cases().at(63));
  source.objects.push_back(source.objects.front());
  rejects(source, OcelEnrichmentGraph::kCodeath);
  source = model(cases().at(63));
  source.events.push_back(source.events.front());
  rejects(source, OcelEnrichmentGraph::kCodeath);
  source = model(cases().at(63));
  source.relations.front().object_id = "absent";
  rejects(source, OcelEnrichmentGraph::kCodeath);
  source = model(cases().at(63));
  source.relations.front().event_id = "absent";
  rejects(source, OcelEnrichmentGraph::kCodeath);
  source = model(cases().at(63));
  rejects(source, static_cast<OcelEnrichmentGraph>(-1));

  for (const auto& spec : cases()) {
    if (spec.change != "empty-object-id" && spec.change != "empty-event-id") continue;
    source = model(spec);
    for (auto graph : {OcelEnrichmentGraph::kInteraction, OcelEnrichmentGraph::kCobirth,
                       OcelEnrichmentGraph::kCodeath, OcelEnrichmentGraph::kDescendants,
                       OcelEnrichmentGraph::kInheritance}) {
      const auto result = discover_ocel_object_graph(source, graph);
      assert(result.graph == graph);
      if (graph == OcelEnrichmentGraph::kDescendants || graph == OcelEnrichmentGraph::kInheritance)
        assert(result.edges.empty());
      else
        assert(result.edges.size() == 3);
    }
    bool threw = false;
    try { (void)enrich_ocel(source); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
}
// Reference importers order events by timestamp (ties keep file order), so
// an object's death is its latest event in time, not in the file. Here o4's
// events are e0 (00:30), e3 (00:30) and e4 (00:03): it dies with o3 at e3.
void imported_events_follow_timestamps() {
  const std::string json = R"({"objectTypes":[{"name":"item","attributes":[]},
    {"name":"pack","attributes":[]}],"eventTypes":[{"name":"B","attributes":[]},
    {"name":"C","attributes":[]}],
    "objects":[{"id":"o3","type":"item"},{"id":"o4","type":"pack"},{"id":"o5","type":"item"}],
    "events":[
      {"id":"e0","type":"B","time":"2024-01-01T00:30:00","relationships":[{"objectId":"o4","qualifier":""}]},
      {"id":"e3","type":"C","time":"2024-01-01T00:30:00","relationships":[{"objectId":"o3","qualifier":""},{"objectId":"o4","qualifier":""}]},
      {"id":"e4","type":"B","time":"2024-01-01T00:03:00","relationships":[{"objectId":"o4","qualifier":""},{"objectId":"o5","qualifier":""}]}]})";
  const auto log = parse_ocel_json(json);
  assert(log.events.size() == 3 && log.events[0].id == "e4" &&
         log.events[1].id == "e0" && log.events[2].id == "e3");
  assert(log.relations.front().event_id == "e4" &&
         log.relations.back().event_id == "e3");
  const auto graph = discover_ocel_object_graph(log, OcelEnrichmentGraph::kCodeath);
  assert(graph.edges == std::vector<Edge>({{"o3", "o4"}}));
}

}  // namespace

int main(int argc, char** argv) {
  const auto failures = retained_public_codeath_cases();
  if (argc == 2 && std::string_view(argv[1]) == "--captured-only")
    return failures ? 1 : 0;
  if (failures) return 1;
  metadata_and_literal_identity_invariance();
  preserved_validation_and_other_selectors();
  imported_events_follow_timestamps();
  std::cout << "Codeath metadata/identity invariance and preserved validation passed\n";
}
