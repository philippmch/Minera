#pragma once

// Writes a document produced by an ostream writer into one std::string of
// exactly its size: a first pass counts the characters, a second appends
// them into the reserved string. A large document then occupies its size
// once, where an ostringstream holds a buffer of up to twice the size and
// str() copies it.

#include <cstddef>
#include <locale>
#include <ostream>
#include <streambuf>
#include <string>

namespace minera::detail {

class CountingStreambuf final : public std::streambuf {
 public:
  std::size_t count = 0;

 protected:
  int_type overflow(int_type ch) override {
    if (!traits_type::eq_int_type(ch, traits_type::eof())) ++count;
    return traits_type::not_eof(ch);
  }
  std::streamsize xsputn(const char*, std::streamsize n) override {
    count += static_cast<std::size_t>(n);
    return n;
  }
};

class AppendingStreambuf final : public std::streambuf {
 public:
  explicit AppendingStreambuf(std::string& out) : out_(out) {}

 protected:
  int_type overflow(int_type ch) override {
    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      out_.push_back(traits_type::to_char_type(ch));
    return traits_type::not_eof(ch);
  }
  std::streamsize xsputn(const char* text, std::streamsize n) override {
    out_.append(text, static_cast<std::size_t>(n));
    return n;
  }

 private:
  std::string& out_;
};

// The size `write(std::ostream&)` produces, written nowhere.
template <class Write>
std::size_t output_size(const Write& write, const std::locale& locale = std::locale()) {
  CountingStreambuf buffer;
  std::ostream out(&buffer);
  out.imbue(locale);
  write(out);
  return buffer.count;
}

// What `write(std::ostream&)` produces, in a string reserved to `size`.
template <class Write>
std::string sized_output(const Write& write, std::size_t size,
                         const std::locale& locale = std::locale()) {
  std::string text;
  text.reserve(size);
  AppendingStreambuf buffer(text);
  std::ostream out(&buffer);
  out.imbue(locale);
  write(out);
  return text;
}

}  // namespace minera::detail
