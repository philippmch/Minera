// Local application server: serves the built frontend and mines uploaded
// CSV event logs with the native core via POST /api/mine.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "httplib.h"
#include "minera/alpha_miner.hpp"
#include "minera/bpmn.hpp"
#include "minera/conformance.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_filter.hpp"
#include "minera/performance.hpp"
#include "minera/petri_net.hpp"
#include "minera/pnml.hpp"
#include "minera/process_tree.hpp"
#include "minera/request_params.hpp"
#include "minera/resource_analysis.hpp"
#include "minera/version.hpp"
#include "minera/xes_event_log_importer.hpp"

namespace {

using minera::error_json;
using minera::looks_like_xes;

// The most recently mined log and its discovered nets, kept so the costly
// alignment of a model can be computed on demand (POST /api/align) instead of
// eagerly for every miner on every mine. The app is local and single-user, so
// caching just the latest analysis is enough; a shared_ptr lets a slow
// alignment run on a snapshot without holding the lock.
struct MinedSession {
  minera::EventLog log;
  minera::DirectlyFollowsGraph graph;
  std::unordered_map<std::string, minera::PetriNet> nets;  // miner id -> net
  minera::ProcessTree tree;  // the inductive model, for on-demand BPMN export
};
std::shared_ptr<const MinedSession> g_session;
std::mutex g_session_mutex;

std::string form_field(const httplib::Request& request,
                       const std::string& name) {
  return request.has_file(name) ? request.get_file_value(name).content
                                : std::string{};
}

void handle_mine(const httplib::Request& request, httplib::Response& response) {
  try {
    std::string csv;
    std::string dataset = "event log";
    minera::MineParams params;

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

      params = minera::parse_mine_params([&](std::string_view name) {
        return form_field(request, std::string(name));
      });
    } else {
      csv = request.body;
    }

    std::istringstream input(csv);
    auto log = looks_like_xes(dataset, csv)
                   ? minera::import_xes_event_log(input)
                   : minera::import_csv_event_log(input, params.csv);
    if (params.filter.any()) {
      log = minera::filter_event_log(log, params.filter);
    }
    const auto graph = minera::discover_directly_follows_graph(log);
    const auto tree = minera::discover_process_tree(log, params.inductive);
    const auto net = minera::discover_petri_net(log);
    const auto heuristics = minera::discover_heuristics_net(log);
    const auto inductive_net = minera::to_petri_net(tree);
    const auto heuristic_net = minera::to_petri_net(heuristics);
    const auto performance = minera::compute_performance_metrics(log, graph);
    const auto variant_durations =
        minera::compute_variant_durations(log, graph);
    const auto resources = minera::discover_resource_network(log);

    // Token replay is cheap and robust, so run it for every model now; defer the
    // costlier alignment to POST /api/align, which reuses the cached nets below.
    const auto model = [&](std::string id, const minera::PetriNet& petri) {
      return minera::ModelConformance{std::move(id),
                                      minera::final_marking_reachable(petri),
                                      minera::check_conformance(log, petri),
                                      minera::check_footprints(log, petri),
                                      std::nullopt};
    };
    const std::vector<minera::ModelConformance> conformance = {
        model("inductive", inductive_net), model("alpha", net),
        model("heuristic", heuristic_net)};

    const std::string body =
        minera::analysis_json(dataset, log, graph, &tree, &net, &heuristics,
                              &conformance, &performance, &resources,
                              &variant_durations);

    // Cache this analysis so alignments can be served on demand for the model
    // the user actually opens, rather than computed for all three up front.
    auto session = std::make_shared<MinedSession>();
    session->log = log;
    session->graph = graph;
    session->nets.emplace("inductive", inductive_net);
    session->nets.emplace("alpha", net);
    session->nets.emplace("heuristic", heuristic_net);
    session->tree = tree;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      g_session = std::move(session);
    }

    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Checks the most recently mined log against a user-supplied PNML workflow
// net (multipart field `model`, or the raw body). The imported net joins the
// cached session under the id "imported", so POST /api/align?miner=imported
// can align against it on demand like any discovered model; the session's
// registry is extended with the net's activities so ids stay resolvable.
void handle_check_model(const httplib::Request& request,
                        httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  try {
    const std::string pnml = request.has_file("model")
                                 ? request.get_file_value("model").content
                                 : request.body;
    if (pnml.empty()) {
      response.status = 400;
      response.set_content(error_json("missing form field: model"),
                           "application/json");
      return;
    }
    const auto imported = minera::parse_pnml(pnml);

    auto next_session = std::make_shared<MinedSession>(*session);
    for (const auto& activity : imported.activities()) {
      next_session->graph.ensure_activity(activity);
    }
    next_session->nets.insert_or_assign("imported", imported);

    const minera::ModelConformance conformance{
        "imported", minera::final_marking_reachable(imported),
        minera::check_conformance(session->log, imported),
        minera::check_footprints(session->log, imported), std::nullopt};
    const auto body =
        minera::imported_model_json(conformance, next_session->graph);

    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      // Only publish if no re-mine replaced the session in the meantime.
      if (g_session == session) g_session = std::move(next_session);
    }
    response.set_content(body, "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// On-demand alignment of one model from the most recent mine. Expects a `miner`
// query parameter ("inductive" | "alpha" | "heuristic").
void handle_align(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.get_param_value("miner");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  try {
    const auto alignment = minera::check_alignment(session->log, it->second);
    response.set_content(minera::alignment_json(alignment, session->graph),
                         "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}

// Exports one model from the most recent mine as standard PNML. Expects a
// `miner` query parameter ("inductive" | "alpha" | "heuristic").
void handle_pnml(const httplib::Request& request, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  const std::string miner = request.get_param_value("miner");
  const auto it = session->nets.find(miner);
  if (it == session->nets.end()) {
    response.status = 400;
    response.set_content(error_json("unknown miner: " + miner),
                         "application/json");
    return;
  }

  response.set_content(minera::to_pnml(it->second, miner + " net"),
                       "application/xml");
}

// Exports the inductive model from the most recent mine as BPMN 2.0. The
// inductive miner is the one that yields a block-structured process tree, which
// maps cleanly onto BPMN, so this endpoint always serves that model.
void handle_bpmn(const httplib::Request&, httplib::Response& response) {
  std::shared_ptr<const MinedSession> session;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    session = g_session;
  }
  if (!session) {
    response.status = 409;  // nothing mined yet this run
    response.set_content(error_json("no mined log; mine a log first"),
                         "application/json");
    return;
  }

  response.set_content(minera::to_bpmn(session->tree, "process"),
                       "application/xml");
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
  const auto preflight = [](const httplib::Request&, httplib::Response& response) {
    response.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
    response.status = 204;
  };
  server.Options("/api/mine", preflight);
  server.Options("/api/align", preflight);
  server.Options("/api/check-model", preflight);

  server.Post("/api/mine", handle_mine);
  server.Post("/api/align", handle_align);
  server.Post("/api/check-model", handle_check_model);
  server.Get("/api/pnml", handle_pnml);
  server.Get("/api/bpmn", handle_bpmn);

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
