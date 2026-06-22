// Local application server: serves the built frontend and mines uploaded
// CSV event logs with the native core via POST /api/mine.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "httplib.h"
#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/version.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {

bool ends_with_ci(std::string_view value, std::string_view suffix) {
  if (value.size() < suffix.size()) return false;
  const auto tail = value.substr(value.size() - suffix.size());
  return std::equal(tail.begin(), tail.end(), suffix.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == b;
  });
}

// Picks the importer by filename extension, falling back to sniffing the
// content for an XML/XES preamble so extension-less uploads still work.
bool looks_like_xes(std::string_view filename, std::string_view content) {
  if (ends_with_ci(filename, ".xes")) return true;
  std::size_t i = 0;
  if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
      static_cast<unsigned char>(content[1]) == 0xBB &&
      static_cast<unsigned char>(content[2]) == 0xBF) {
    i = 3;
  }
  while (i < content.size() &&
         std::isspace(static_cast<unsigned char>(content[i]))) {
    ++i;
  }
  const auto rest = content.substr(i);
  return rest.rfind("<?xml", 0) == 0 || rest.rfind("<log", 0) == 0;
}

std::vector<std::string> split_csv(std::string_view value) {
  std::vector<std::string> parts;
  std::string current;
  for (const char ch : value) {
    if (ch == ',') {
      if (!current.empty()) parts.push_back(current);
      current.clear();
    } else if (ch != ' ' || !current.empty()) {
      current.push_back(ch);
    }
  }
  if (!current.empty()) parts.push_back(current);
  return parts;
}

std::string error_json(std::string_view message) {
  std::ostringstream out;
  out << "{\"error\": \"";
  for (const char c : message) {
    if (c == '"' || c == '\\') out << '\\';
    out << c;
  }
  out << "\"}";
  return out.str();
}

std::string form_field(const httplib::Request& request,
                       const std::string& name) {
  return request.has_file(name) ? request.get_file_value(name).content
                                : std::string{};
}

void handle_mine(const httplib::Request& request, httplib::Response& response) {
  try {
    std::string csv;
    std::string dataset = "event log";
    minera::CsvImportOptions options;
    minera::LogFilterOptions filter;

    if (request.is_multipart_form_data()) {
      if (!request.has_file("log")) {
        response.status = 400;
        response.set_content(error_json("missing form field: log"),
                             "application/json");
        return;
      }
      const auto& file = request.get_file_value("log");
      csv = file.content;
      if (!file.filename.empty()) dataset = file.filename;

      if (const auto value = form_field(request, "case_column"); !value.empty()) {
        options.case_id_column = value;
      }
      if (const auto value = form_field(request, "activity_column"); !value.empty()) {
        options.activity_column = value;
      }
      if (const auto value = form_field(request, "timestamp_column"); !value.empty()) {
        options.timestamp_column = value;
      }
      if (const auto value = form_field(request, "delimiter"); !value.empty()) {
        options.delimiter = value.front();
      }

      // Optional log-preprocessing filters, applied before discovery.
      if (const auto value = form_field(request, "keep_activities"); !value.empty()) {
        filter.keep_activities = split_csv(value);
      }
      if (const auto value = form_field(request, "remove_activities"); !value.empty()) {
        filter.remove_activities = split_csv(value);
      }
      if (const auto value = form_field(request, "start_activities"); !value.empty()) {
        filter.start_activities = split_csv(value);
      }
      if (const auto value = form_field(request, "end_activities"); !value.empty()) {
        filter.end_activities = split_csv(value);
      }
      if (const auto value = form_field(request, "min_length"); !value.empty()) {
        filter.min_length = static_cast<std::size_t>(std::atoll(value.c_str()));
      }
      if (const auto value = form_field(request, "max_length"); !value.empty()) {
        filter.max_length = static_cast<std::size_t>(std::atoll(value.c_str()));
      }
      if (const auto value = form_field(request, "min_variant_frequency"); !value.empty()) {
        filter.min_variant_frequency =
            static_cast<std::uint64_t>(std::atoll(value.c_str()));
      }
      if (const auto value = form_field(request, "top_variants"); !value.empty()) {
        filter.top_variants = static_cast<std::size_t>(std::atoll(value.c_str()));
      }
    } else {
      csv = request.body;
    }

    std::istringstream input(csv);
    auto log = looks_like_xes(dataset, csv)
                   ? minera::import_xes_event_log(input)
                   : minera::import_csv_event_log(input, options);
    if (filter.any()) {
      log = minera::filter_event_log(log, filter);
    }
    const auto graph = minera::discover_directly_follows_graph(log);
    const auto tree = minera::discover_process_tree(log);
    const auto net = minera::discover_petri_net(log);
    const auto heuristics = minera::discover_heuristics_net(log);

    // Check every discovered model both ways, so the frontend can offer any
    // miner against any conformance method.
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const std::vector<minera::ModelConformance> conformance = {
        {"inductive", minera::check_conformance(log, inductive_net),
         minera::check_alignment(log, inductive_net)},
        {"alpha", minera::check_conformance(log, net),
         minera::check_alignment(log, net)},
        {"heuristic", minera::check_conformance(log, heuristic_net),
         minera::check_alignment(log, heuristic_net)},
    };
    response.set_content(
        minera::analysis_json(dataset, log, graph, &tree, &net, &heuristics,
                              &conformance),
        "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

int usage(const char* program) {
  std::cerr << "Usage: " << program
            << " [options]\n"
               "  --port <port>      Listen port (default: 8484)\n"
               "  --frontend <dir>   Static frontend directory"
               " (default: frontend/dist next to the working directory)\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  int port = 8484;
  std::filesystem::path frontend_dir = "frontend/dist";

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const auto next = [&]() -> const char* {
      return i + 1 < argc ? argv[++i] : nullptr;
    };

    if (arg == "--port") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      port = std::atoi(value);
      if (port <= 0 || port > 65535) return usage(argv[0]);
    } else if (arg == "--frontend") {
      const auto* value = next();
      if (!value) return usage(argv[0]);
      frontend_dir = value;
    } else {
      return usage(argv[0]);
    }
  }

  httplib::Server server;

  // The app is local-first; permissive CORS keeps the Vite dev server (a
  // different port) working against this backend.
  server.set_default_headers({{"Access-Control-Allow-Origin", "*"}});
  server.Options("/api/mine", [](const httplib::Request&, httplib::Response& response) {
    response.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
    response.status = 204;
  });

  server.Post("/api/mine", handle_mine);

  server.Get("/api/version", [](const httplib::Request&, httplib::Response& response) {
    std::ostringstream out;
    out << "{\"generator\": \"minera " << minera::version_major << '.'
        << minera::version_minor << '.' << minera::version_patch << "\"}";
    response.set_content(out.str(), "application/json");
  });

  const bool has_frontend = std::filesystem::is_directory(frontend_dir);
  if (has_frontend) {
    server.set_mount_point("/", frontend_dir.string());
  }

  std::cout << "minera " << minera::version_major << '.' << minera::version_minor
            << '.' << minera::version_patch << " — listening on http://localhost:"
            << port << '\n';
  if (!has_frontend) {
    std::cout << "note: frontend directory not found (" << frontend_dir.string()
              << "); serving the API only\n";
  }

  if (!server.listen("127.0.0.1", port)) {
    std::cerr << "error: cannot listen on port " << port << '\n';
    return 1;
  }
  return 0;
}
