#include "minera/streaming_store.hpp"

#include <fstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace minera {
namespace {

using StateRow = std::pair<std::string, std::string>;

void write_csv_field(std::ostream& out, std::string_view value) {
  const bool quoted = value.find_first_of(",\"\r\n") != std::string_view::npos;
  if (!quoted) {
    out << value;
    return;
  }
  out << '"';
  for (const char character : value) {
    if (character == '"') out << "\"\"";
    else out << character;
  }
  out << '"';
}

void write_header(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("cannot create streaming DFG state file");
  output << "case_id,activity\n";
  if (!output) throw std::runtime_error("cannot initialize streaming DFG state file");
}

std::vector<StateRow> read_state_rows(std::istream& input) {
  const std::string body((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  std::vector<StateRow> rows;
  std::vector<std::string> fields;
  std::string field;
  bool in_quotes = false;
  bool after_quote = false;
  bool field_start = true;

  const auto finish_row = [&]() {
    fields.push_back(std::move(field));
    field.clear();
    if (fields.size() != 2)
      throw std::invalid_argument("streaming DFG state row must have two fields");
    // Empty case ids and activities are ordinary values, as in the reference.
    rows.emplace_back(std::move(fields[0]), std::move(fields[1]));
    fields.clear();
    field_start = true;
    after_quote = false;
  };

  for (std::size_t i = 0; i < body.size(); ++i) {
    const char character = body[i];
    if (in_quotes) {
      if (character != '"') {
        field.push_back(character);
      } else if (i + 1 < body.size() && body[i + 1] == '"') {
        field.push_back('"');
        ++i;
      } else {
        in_quotes = false;
        after_quote = true;
      }
      continue;
    }

    if (after_quote) {
      if (character == ',') {
        fields.push_back(std::move(field));
        field.clear();
        field_start = true;
        after_quote = false;
      } else if (character == '\n' || character == '\r') {
        if (character == '\r' && i + 1 < body.size() && body[i + 1] == '\n')
          ++i;
        finish_row();
      } else {
        throw std::invalid_argument(
            "streaming DFG state has data after a quoted field");
      }
      continue;
    }

    if (character == '"') {
      if (!field_start)
        throw std::invalid_argument("streaming DFG state has an invalid quote");
      in_quotes = true;
      field_start = false;
    } else if (character == ',') {
      fields.push_back(std::move(field));
      field.clear();
      field_start = true;
    } else if (character == '\n' || character == '\r') {
      if (character == '\r' && i + 1 < body.size() && body[i + 1] == '\n')
        ++i;
      finish_row();
    } else {
      field.push_back(character);
      field_start = false;
    }
  }

  if (in_quotes)
    throw std::invalid_argument("streaming DFG state has an unterminated quote");
  if (after_quote || !fields.empty() || !field.empty()) finish_row();
  return rows;
}

}  // namespace

PersistentStreamingDfg::PersistentStreamingDfg(std::filesystem::path path,
                                               StreamingDfgOptions options)
    : path_(std::move(path)), discovery_(std::move(options)) {
  if (path_.empty())
    throw std::invalid_argument("streaming DFG state path must not be empty");

  std::error_code error;
  const bool exists = std::filesystem::exists(path_, error);
  if (error) throw std::runtime_error("cannot inspect streaming DFG state file");
  if (!exists) {
    write_header(path_);
    return;
  }

  const auto size = std::filesystem::file_size(path_, error);
  if (error) throw std::runtime_error("cannot inspect streaming DFG state file");
  if (size == 0) {
    write_header(path_);
    return;
  }

  std::ifstream input(path_, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open streaming DFG state file");
  std::string header;
  std::getline(input, header);
  if (!header.empty() && header.back() == '\r') header.pop_back();
  if (header != "case_id,activity")
    throw std::invalid_argument("invalid streaming DFG state file header");

  for (const auto& [case_id, activity] : read_state_rows(input))
    if (!discovery_.receive(case_id, activity))
      throw std::invalid_argument("streaming DFG state contains an invalid event");
}

void PersistentStreamingDfg::append(std::string_view case_id,
                                    std::string_view activity) {
  std::ofstream output(path_, std::ios::binary | std::ios::app);
  if (!output) throw std::runtime_error("cannot append streaming DFG state file");
  write_csv_field(output, case_id);
  output << ',';
  write_csv_field(output, activity);
  output << '\n';
  if (!output) throw std::runtime_error("cannot append streaming DFG state file");
}

bool PersistentStreamingDfg::receive(std::string_view case_id,
                                     std::string_view activity) {
  std::lock_guard<std::mutex> lock(mutex_);
  append(case_id, activity);
  return discovery_.receive(case_id, activity);
}

bool PersistentStreamingDfg::receive(const Event& event,
                                     std::string_view case_id) {
  const auto activity = streaming_event_activity(event, discovery_.options().activity_attribute);
  return activity && receive(case_id, *activity);
}

StreamingDfgSnapshot PersistentStreamingDfg::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return discovery_.snapshot();
}

void PersistentStreamingDfg::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  discovery_.reset();
  write_header(path_);
}

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

void ensure_socket_runtime() {
  static std::once_flag once;
  std::call_once(once, [] {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
      throw std::runtime_error("could not initialize socket runtime");
  });
}

int socket_error() { return WSAGetLastError(); }
bool connect_in_progress(int error) {
  return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS ||
         error == WSAEALREADY;
}
void close_socket(NativeSocket socket) { closesocket(socket); }
void set_blocking(NativeSocket socket, bool blocking) {
  u_long mode = blocking ? 0UL : 1UL;
  if (ioctlsocket(socket, FIONBIO, &mode) != 0)
    throw std::runtime_error("could not configure remote store socket");
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;

void ensure_socket_runtime() {}
int socket_error() { return errno; }
bool connect_in_progress(int error) {
  return error == EINPROGRESS || error == EALREADY || error == EWOULDBLOCK;
}
void close_socket(NativeSocket socket) { close(socket); }
void set_blocking(NativeSocket socket, bool blocking) {
  const int flags = fcntl(socket, F_GETFL, 0);
  if (flags < 0 || fcntl(socket, F_SETFL,
                         blocking ? (flags & ~O_NONBLOCK)
                                  : (flags | O_NONBLOCK)) < 0)
    throw std::runtime_error("could not configure remote store socket");
}
#endif

void set_socket_timeout(NativeSocket socket, std::uint32_t timeout_ms) {
#ifdef _WIN32
  const DWORD timeout = timeout_ms;
  if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout)) !=
          0 ||
      setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout)) !=
          0)
#else
  timeval timeout{};
  timeout.tv_sec = static_cast<long>(timeout_ms / 1000);
  timeout.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
  if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) !=
          0 ||
      setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) !=
          0)
#endif
    throw std::runtime_error("could not configure remote store timeout");
}

class SocketConnection {
 public:
  SocketConnection(std::string_view hostname, std::uint16_t port,
                   std::uint32_t timeout_ms) {
    ensure_socket_runtime();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    const auto port_text = std::to_string(port);
    addrinfo* addresses = nullptr;
    const int lookup = getaddrinfo(std::string(hostname).c_str(),
                                   port_text.c_str(), &hints, &addresses);
    if (lookup != 0)
      throw std::runtime_error("could not resolve remote store host");

    for (addrinfo* address = addresses; address != nullptr;
         address = address->ai_next) {
      const NativeSocket candidate =
          static_cast<NativeSocket>(socket(address->ai_family,
                                           address->ai_socktype,
                                           address->ai_protocol));
      if (candidate == kInvalidSocket) continue;
      try {
        set_blocking(candidate, false);
        const int connected = connect(candidate, address->ai_addr,
                                      static_cast<int>(address->ai_addrlen));
        if (connected != 0) {
          const int error = socket_error();
          if (!connect_in_progress(error)) throw std::runtime_error("connect failed");
          fd_set writable;
          FD_ZERO(&writable);
          FD_SET(candidate, &writable);
          timeval timeout{};
          timeout.tv_sec = static_cast<long>(timeout_ms / 1000);
          timeout.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
#ifdef _WIN32
          const int selected = select(0, nullptr, &writable, nullptr, &timeout);
#else
          const int selected = select(candidate + 1, nullptr, &writable,
                                      nullptr, &timeout);
#endif
          if (selected <= 0) throw std::runtime_error("remote store connect timeout");
          int connect_error = 0;
#ifdef _WIN32
          int length = sizeof(connect_error);
          if (getsockopt(candidate, SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&connect_error), &length) != 0)
#else
          socklen_t length = sizeof(connect_error);
          if (getsockopt(candidate, SOL_SOCKET, SO_ERROR, &connect_error,
                         &length) != 0)
#endif
            throw std::runtime_error("could not inspect remote store socket");
          if (connect_error != 0) throw std::runtime_error("remote store connect failed");
        }
        set_blocking(candidate, true);
        set_socket_timeout(candidate, timeout_ms);
        socket_ = candidate;
        break;
      } catch (...) {
        close_socket(candidate);
      }
    }
    freeaddrinfo(addresses);
    if (socket_ == kInvalidSocket)
      throw std::runtime_error("could not connect to remote store");
  }

  ~SocketConnection() {
    if (socket_ != kInvalidSocket) close_socket(socket_);
  }
  SocketConnection(const SocketConnection&) = delete;
  SocketConnection& operator=(const SocketConnection&) = delete;

  void write_all(std::string_view data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
#ifdef _WIN32
      const int sent = send(socket_, data.data() + offset,
                            static_cast<int>(data.size() - offset), 0);
#else
      const ssize_t sent = send(socket_, data.data() + offset,
                                data.size() - offset, MSG_NOSIGNAL);
#endif
      if (sent <= 0) throw std::runtime_error("remote store write failed");
      offset += static_cast<std::size_t>(sent);
    }
  }

  std::string read_exact(std::size_t size) {
    std::string result(size, '\0');
    std::size_t offset = 0;
    while (offset < size) {
#ifdef _WIN32
      const int received = recv(socket_, result.data() + offset,
                                static_cast<int>(size - offset), 0);
#else
      const ssize_t received = recv(socket_, result.data() + offset,
                                    size - offset, 0);
#endif
      if (received <= 0) throw std::runtime_error("remote store read failed");
      offset += static_cast<std::size_t>(received);
    }
    return result;
  }

  std::string read_line() {
    std::string line;
    line.reserve(64);
    for (;;) {
      char character = '\0';
#ifdef _WIN32
      const int received = recv(socket_, &character, 1, 0);
#else
      const ssize_t received = recv(socket_, &character, 1, 0);
#endif
      if (received <= 0) throw std::runtime_error("remote store read failed");
      if (character == '\n') {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return line;
      }
      if (line.size() >= 8 * 1024 * 1024)
        throw std::runtime_error("remote store response line is too large");
      line.push_back(character);
    }
  }

 private:
  NativeSocket socket_ = kInvalidSocket;
};

struct RedisReply {
  char kind = 'n';
  std::string text;
  std::vector<RedisReply> items;
  std::int64_t integer = 0;
};

std::int64_t parse_reply_integer(std::string_view value) {
  if (value.empty()) throw std::runtime_error("invalid remote store response");
  std::size_t position = 0;
  bool negative = false;
  if (value.front() == '-') {
    negative = true;
    position = 1;
  }
  if (position == value.size())
    throw std::runtime_error("invalid remote store response");
  std::int64_t result = 0;
  for (; position < value.size(); ++position) {
    const char digit = value[position];
    if (digit < '0' || digit > '9')
      throw std::runtime_error("invalid remote store response");
    const auto value_digit = static_cast<std::int64_t>(digit - '0');
    if (result > (std::numeric_limits<std::int64_t>::max() - value_digit) / 10)
      throw std::runtime_error("remote store integer overflow");
    result = result * 10 + value_digit;
  }
  return negative ? -result : result;
}

RedisReply read_reply(SocketConnection& connection, std::size_t depth = 0) {
  if (depth > 64) throw std::runtime_error("remote store response is too deep");
  const auto line = connection.read_line();
  if (line.empty()) throw std::runtime_error("invalid remote store response");
  const char kind = line.front();
  const std::string_view value(line.data() + 1, line.size() - 1);
  if (kind == '+' || kind == '-') {
    if (kind == '-') throw std::runtime_error(std::string("remote store: ") + std::string(value));
    return RedisReply{kind, std::string(value), {}, 0};
  }
  if (kind == ':') return RedisReply{kind, {}, {}, parse_reply_integer(value)};
  if (kind == '$') {
    const auto length = parse_reply_integer(value);
    if (length == -1) return RedisReply{'n', {}, {}, 0};
    if (length < 0 || length > 64 * 1024 * 1024)
      throw std::runtime_error("remote store bulk response is too large");
    auto text = connection.read_exact(static_cast<std::size_t>(length));
    if (connection.read_exact(2) != "\r\n")
      throw std::runtime_error("invalid remote store bulk response");
    return RedisReply{'$', std::move(text), {}, 0};
  }
  if (kind == '*') {
    const auto length = parse_reply_integer(value);
    if (length == -1) return RedisReply{'n', {}, {}, 0};
    if (length < 0 || length > 1'000'000)
      throw std::runtime_error("remote store array response is too large");
    RedisReply result{'*', {}, {}, 0};
    result.items.reserve(static_cast<std::size_t>(length));
    for (std::int64_t i = 0; i < length; ++i)
      result.items.push_back(read_reply(connection, depth + 1));
    return result;
  }
  throw std::runtime_error("unknown remote store response type");
}

std::string encode_command(const std::vector<std::string>& arguments) {
  std::ostringstream out;
  out << '*' << arguments.size() << "\r\n";
  for (const auto& argument : arguments)
    out << '$' << argument.size() << "\r\n" << argument << "\r\n";
  return out.str();
}

RedisReply remote_command(const StreamingDictionaryOptions& options,
                          std::vector<std::string> arguments) {
  SocketConnection connection(options.hostname, options.port,
                              options.timeout_ms);
  connection.write_all(encode_command({"SELECT", std::to_string(options.database)}));
  (void)read_reply(connection);
  connection.write_all(encode_command(arguments));
  return read_reply(connection);
}

void require_key(std::string_view key) {
  if (key.empty()) throw std::invalid_argument("streaming dictionary key must not be empty");
}

std::string remote_key(const StreamingDictionaryOptions& options,
                       std::string_view key) {
  require_key(key);
  return options.key_prefix + std::string(key);
}

std::vector<std::string> reply_strings(const RedisReply& reply) {
  if (reply.kind != '*') throw std::runtime_error("remote store expected an array");
  std::vector<std::string> values;
  values.reserve(reply.items.size());
  for (const auto& item : reply.items) {
    if (item.kind != '$' && item.kind != '+')
      throw std::runtime_error("remote store returned a non-string item");
    values.push_back(item.text);
  }
  return values;
}

}  // namespace

struct StreamingDictionary::Impl {
  explicit Impl(StreamingDictionaryOptions value) : options(std::move(value)) {
    if (options.variant == StreamingDictionaryVariant::kRedis) {
      if (options.hostname.empty())
        throw std::invalid_argument("remote store hostname must not be empty");
      if (options.port == 0)
        throw std::invalid_argument("remote store port must be positive");
      if (options.timeout_ms == 0 || options.timeout_ms > 60'000)
        throw std::invalid_argument("remote store timeout must be from 1 through 60000 ms");
      if (options.key_prefix.find_first_of("*?[]") != std::string::npos)
        throw std::invalid_argument("remote store key prefix contains a glob character");
    }
  }

  StreamingDictionaryOptions options;
  mutable std::mutex mutex;
  std::map<std::string, std::string> values;
};

StreamingDictionary::StreamingDictionary(StreamingDictionaryOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

StreamingDictionary::~StreamingDictionary() = default;
StreamingDictionary::StreamingDictionary(StreamingDictionary&&) noexcept = default;
StreamingDictionary& StreamingDictionary::operator=(StreamingDictionary&&) noexcept = default;

StreamingDictionaryVariant StreamingDictionary::variant() const noexcept {
  return impl_->options.variant;
}

const StreamingDictionaryOptions& StreamingDictionary::options() const noexcept {
  return impl_->options;
}

void StreamingDictionary::set(std::string_view key, std::string_view value) {
  require_key(key);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis) {
    impl_->values[std::string(key)] = std::string(value);
    return;
  }
  const auto reply = remote_command(impl_->options,
                                    {"SET", remote_key(impl_->options, key),
                                     std::string(value)});
  if (reply.kind != '+') throw std::runtime_error("remote store did not accept SET");
}

std::optional<std::string> StreamingDictionary::get(std::string_view key) const {
  require_key(key);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis) {
    const auto it = impl_->values.find(std::string(key));
    return it == impl_->values.end() ? std::nullopt
                                     : std::optional<std::string>(it->second);
  }
  const auto reply = remote_command(impl_->options,
                                    {"GET", remote_key(impl_->options, key)});
  if (reply.kind == 'n') return std::nullopt;
  if (reply.kind != '$' && reply.kind != '+')
    throw std::runtime_error("remote store returned an invalid GET response");
  return reply.text;
}

bool StreamingDictionary::contains(std::string_view key) const {
  require_key(key);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis)
    return impl_->values.contains(std::string(key));
  const auto reply = remote_command(impl_->options,
                                    {"EXISTS", remote_key(impl_->options, key)});
  if (reply.kind != ':') throw std::runtime_error("remote store returned an invalid EXISTS response");
  return reply.integer != 0;
}

bool StreamingDictionary::erase(std::string_view key) {
  require_key(key);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis)
    return impl_->values.erase(std::string(key)) != 0;
  const auto reply = remote_command(impl_->options,
                                    {"DEL", remote_key(impl_->options, key)});
  if (reply.kind != ':') throw std::runtime_error("remote store returned an invalid DEL response");
  return reply.integer != 0;
}

std::vector<std::string> StreamingDictionary::keys() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis) {
    std::vector<std::string> result;
    result.reserve(impl_->values.size());
    for (const auto& [key, value] : impl_->values) {
      (void)value;
      result.push_back(key);
    }
    return result;
  }
  const auto reply = remote_command(
      impl_->options, {"KEYS", impl_->options.key_prefix + "*"});
  auto result = reply_strings(reply);
  for (auto& key : result)
    if (key.starts_with(impl_->options.key_prefix))
      key.erase(0, impl_->options.key_prefix.size());
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::string> StreamingDictionary::values() const {
  const auto names = keys();
  std::vector<std::string> result;
  result.reserve(names.size());
  for (const auto& key : names) {
    const auto value = get(key);
    if (value) result.push_back(*value);
  }
  return result;
}

std::size_t StreamingDictionary::size() const { return keys().size(); }

void StreamingDictionary::clear() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis) {
    impl_->values.clear();
    return;
  }
  if (impl_->options.key_prefix.empty())
    throw std::invalid_argument("remote store clear requires a non-empty key prefix");
  const auto names = reply_strings(remote_command(
      impl_->options, {"KEYS", impl_->options.key_prefix + "*"}));
  for (std::size_t begin = 0; begin < names.size(); begin += 512) {
    std::vector<std::string> command{"DEL"};
    const auto end = std::min(names.size(), begin + 512);
    command.insert(command.end(), names.begin() + static_cast<std::ptrdiff_t>(begin),
                   names.begin() + static_cast<std::ptrdiff_t>(end));
    (void)remote_command(impl_->options, std::move(command));
  }
}

void StreamingDictionary::flush_database() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis) {
    impl_->values.clear();
    return;
  }
  const auto reply = remote_command(impl_->options, {"FLUSHDB"});
  if (reply.kind != '+') throw std::runtime_error("remote store did not accept FLUSHDB");
}

void StreamingDictionary::flush_all() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (variant() != StreamingDictionaryVariant::kRedis) {
    impl_->values.clear();
    return;
  }
  const auto reply = remote_command(impl_->options, {"FLUSHALL"});
  if (reply.kind != '+') throw std::runtime_error("remote store did not accept FLUSHALL");
}

bool StreamingDictionary::available() const noexcept {
  if (variant() != StreamingDictionaryVariant::kRedis) return true;
  try {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto reply = remote_command(impl_->options, {"PING"});
    return reply.kind == '+' && (reply.text == "PONG" || reply.text == "OK");
  } catch (...) {
    return false;
  }
}

}  // namespace minera
