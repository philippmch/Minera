// GET /api/traces?source_id=: the traces of the active source's analysis on
// demand. A client that mines with include_traces=false gets a mining
// response without the "traces" array (about two thirds of a large body) and
// a "trace_summary" in its place; the views that need the cases fetch them
// here. The "traces" value is the array /api/mine writes inline, byte for
// byte: the same serializer over the session's log and graph.
// Included by minera_server.cpp after active_source_route.hpp.

// A large JSON body, gzipped at the fastest level when the client accepts
// it (httplib would deflate a held body at its slower default level), and
// streamed from a shared buffer as the mining body is.
void deliver_large_json(const httplib::Request& request, httplib::Response& response,
                        std::string body) {
  if (!minera::detail::is_clean_utf8(body)) body = active_source_safe_json(body);
  const std::string type = "application/json";
#if defined(CPPHTTPLIB_ZLIB_SUPPORT)
  response.set_header("Content-Type", type);
  if (request.ranges.empty() &&
      httplib::detail::encoding_type(request, response) == httplib::detail::EncodingType::Gzip) {
    if (auto compressed = gzip_mining_body(body)) {
      body = std::move(*compressed);
      response.set_header("Content-Encoding", "gzip");
    }
  }
  response.headers.erase("Content-Type");
#else
  (void)request;
#endif
  auto shared = std::make_shared<std::string>(std::move(body));
  const auto size = shared->size();
  response.set_content_provider(size, type,
      [shared](std::size_t offset, std::size_t length, httplib::DataSink& sink) {
        return sink.write(shared->data() + offset, std::min(length, shared->size() - offset));
      },
      [shared](bool) mutable noexcept { shared.reset(); });
}

void handle_traces(const httplib::Request& request, httplib::Response& response) {
  try {
    for (const auto& [key, value] : request.params) {
      (void)value;
      if (key != "source_id") throw std::invalid_argument("unknown traces parameter: " + key);
    }
    if (!request.has_param("source_id"))
      throw std::invalid_argument("source_id is required for /api/traces");
    std::shared_ptr<const MinedSession> session;
    { std::lock_guard<std::mutex> lock(g_session_mutex); session = g_session; }
    if (!session) {
      response.status = 409;
      response.set_content(error_json("no mined log; mine a log first"), "application/json");
      return;
    }
    const auto source_id = std::to_string(session->query_source_id);
    if (request.get_param_value("source_id") != source_id) {
      response.status = 409;
      response.set_content(error_json("the source log changed; reload its traces"), "application/json");
      return;
    }
    const auto& log = require_prepared_log(*session);
    std::string body = minera::analysis_traces_json(log, require_graph(*session));
    const std::string head = "{\"family\":\"ANALYSIS_TRACES\",\"source_id\":" + active_source_quote(source_id) +
        ",\"trace_count\":" + std::to_string(log.trace_count()) +
        ",\"event_count\":" + std::to_string(log.event_count()) + ",\"traces\":";
    body.reserve(head.size() + body.size() + 1);
    body.insert(0, head);
    body += '}';
    deliver_large_json(request, response, std::move(body));
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(error_json(error.what()), "application/json");
  }
}
