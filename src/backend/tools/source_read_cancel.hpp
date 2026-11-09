#pragma once

// A cancel-mine sent while POST /api/mine reads its upload stops the read
// part-way. prepare_active_event_source reads the upload through
// SourceReadStream; its buffer hands the bytes out in 64 KiB slices and, when
// handle_mine has set a read-cancel hook for this thread, asks the hook before
// each slice and throws minera::OperationCancelled once it answers true. A
// thread without a hook (every other route) reads the same bytes unchanged.
// Included in the server implementation namespace, after the standard headers.

inline thread_local const std::function<bool()>* t_source_read_cancelled = nullptr;

// Sets the read-cancel hook for the current thread while it lives.
class SourceReadCancelScope {
 public:
  explicit SourceReadCancelScope(const std::function<bool()>& cancelled)
      : previous_(t_source_read_cancelled) {t_source_read_cancelled = &cancelled;}
  ~SourceReadCancelScope() {t_source_read_cancelled = previous_;}
  SourceReadCancelScope(const SourceReadCancelScope&) = delete;
  SourceReadCancelScope& operator=(const SourceReadCancelScope&) = delete;

 private:
  const std::function<bool()>* previous_;
};

// Throws when the current thread's read-cancel hook answers true; the
// importer's own work after the last slice (grouping events into cases) is
// followed by this check before the source is built.
inline void check_source_read_cancelled() {
  if (t_source_read_cancelled)
    minera::throw_if_cancelled(*t_source_read_cancelled,
                               "mining request was cancelled while reading the event log");
}

// A read-only view of `bytes` (which must outlive it). The get area always
// starts at the first byte, so putback and seeking behave as in a stringbuf.
class SourceReadBuffer : public std::streambuf {
 public:
  explicit SourceReadBuffer(const std::string& bytes)
      : begin_(const_cast<char*>(bytes.data())), end_(begin_ + bytes.size()) {
    setg(begin_, begin_, begin_ + std::min<std::size_t>(kSlice, bytes.size()));
  }

 protected:
  int_type underflow() override {
    if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
    if (egptr() >= end_) return traits_type::eof();
    check_source_read_cancelled();
    const auto next = egptr();
    setg(begin_, next, next + std::min<std::size_t>(kSlice, static_cast<std::size_t>(end_ - next)));
    return traits_type::to_int_type(*gptr());
  }
  std::streamsize showmanyc() override {return end_ - gptr();}
  pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode which) override {
    if (!(which & std::ios_base::in)) return pos_type(off_type(-1));
    const off_type base = way == std::ios_base::beg ? 0 : way == std::ios_base::cur ? gptr() - begin_ : end_ - begin_;
    return seekpos(pos_type(base + offset), which);
  }
  pos_type seekpos(pos_type position, std::ios_base::openmode which) override {
    const off_type at = off_type(position);
    if (!(which & std::ios_base::in) || at < 0 || at > end_ - begin_) return pos_type(off_type(-1));
    setg(begin_, begin_ + at, std::max(egptr(), std::min(begin_ + at + static_cast<off_type>(kSlice), end_)));
    return position;
  }

 private:
  static constexpr std::size_t kSlice = 1u << 16;
  char* begin_;
  char* end_;
};

// The upload as an input stream. A cancel thrown by the buffer propagates
// out of the importer (badbit is an exception here) instead of reading as a
// truncated file.
class SourceReadStream : public std::istream {
 public:
  explicit SourceReadStream(const std::string& bytes) : std::istream(nullptr), buffer_(bytes) {
    rdbuf(&buffer_);
    exceptions(std::ios_base::badbit);
  }

 private:
  SourceReadBuffer buffer_;
};
