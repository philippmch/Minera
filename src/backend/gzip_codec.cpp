#include "gzip_codec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(MINERA_HAS_ZLIB)
#include <zlib.h>
#endif

namespace minera::detail {
namespace {

std::uint32_t crc32(std::string_view bytes) noexcept {
  std::uint32_t crc = 0xffffffffU;
  for (const unsigned char byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return crc ^ 0xffffffffU;
}

void append_u16(std::string& out, std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xffU));
  out.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    out.push_back(static_cast<char>((value >> shift) & 0xffU));
}

std::uint16_t read_u16(std::string_view bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 2)
    throw std::runtime_error("gzip input is truncated");
  return static_cast<std::uint16_t>(
      static_cast<unsigned char>(bytes[offset]) |
      (static_cast<unsigned char>(bytes[offset + 1]) << 8U));
}

std::uint32_t read_u32(std::string_view bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 4)
    throw std::runtime_error("gzip input is truncated");
  std::uint32_t value = 0;
  for (int shift = 0; shift < 32; shift += 8)
    value |= static_cast<std::uint32_t>(
                 static_cast<unsigned char>(bytes[offset + shift / 8]))
             << shift;
  return value;
}

class BitReader {
 public:
  BitReader(std::string_view bytes, std::size_t offset)
      : bytes_(bytes), byte_(offset) {}

  std::uint32_t read_bits(unsigned count) {
    if (count > 24) throw std::runtime_error("deflate bit request is invalid");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
      if (byte_ >= bytes_.size())
        throw std::runtime_error("deflate stream is truncated");
      value |= ((static_cast<unsigned char>(bytes_[byte_]) >> bit_) & 1U) << i;
      if (++bit_ == 8) {
        bit_ = 0;
        ++byte_;
      }
    }
    return value;
  }

  void align_byte() {
    if (bit_ != 0) {
      bit_ = 0;
      ++byte_;
    }
  }

  [[nodiscard]] std::size_t byte_position() const noexcept { return byte_; }

 private:
  std::string_view bytes_;
  std::size_t byte_ = 0;
  unsigned bit_ = 0;
};

struct HuffmanNode {
  std::array<int, 2> child{-1, -1};
  int symbol = -1;
};

class HuffmanTree {
 public:
  explicit HuffmanTree(const std::vector<unsigned>& lengths,
                       bool allow_empty = false) {
    std::array<unsigned, 16> counts{};
    unsigned used = 0;
    for (const auto length : lengths) {
      if (length > 15) throw std::runtime_error("deflate code is too long");
      if (length != 0) {
        ++counts[length];
        ++used;
      }
    }
    if (used == 0) {
      if (allow_empty) return;
      throw std::runtime_error("deflate Huffman tree is empty");
    }
    int available = 1;
    for (unsigned length = 1; length <= 15; ++length) {
      available = available * 2 - static_cast<int>(counts[length]);
      if (available < 0)
        throw std::runtime_error("deflate Huffman tree is oversubscribed");
    }

    std::array<unsigned, 16> next_code{};
    unsigned code = 0;
    for (unsigned length = 1; length <= 15; ++length) {
      code = (code + counts[length - 1]) << 1U;
      next_code[length] = code;
    }

    nodes_.push_back({});
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
      const auto length = lengths[symbol];
      if (length == 0) continue;
      const auto canonical = next_code[length]++;
      int node = 0;
      for (unsigned i = 0; i < length; ++i) {
        // Deflate writes Huffman codes most-significant bit first, even though
        // the surrounding packed fields are read least-significant bit first.
        const auto bit = static_cast<int>(
            (canonical >> (length - i - 1U)) & 1U);
        auto child = nodes_[node].child[bit];
        if (child == -1) {
          child = static_cast<int>(nodes_.size());
          nodes_[node].child[bit] = child;
          nodes_.push_back({});
        }
        node = child;
      }
      if (nodes_[node].symbol != -1)
        throw std::runtime_error("deflate Huffman code is duplicated");
      nodes_[node].symbol = static_cast<int>(symbol);
    }
  }

  [[nodiscard]] bool empty() const noexcept { return nodes_.empty(); }

  int decode(BitReader& reader) const {
    if (nodes_.empty()) throw std::runtime_error("deflate distance tree is empty");
    int node = 0;
    for (unsigned depth = 0; depth < 15; ++depth) {
      const auto bit = static_cast<int>(reader.read_bits(1));
      node = nodes_[node].child[bit];
      if (node < 0) throw std::runtime_error("deflate Huffman code is invalid");
      if (nodes_[node].symbol >= 0) return nodes_[node].symbol;
    }
    throw std::runtime_error("deflate Huffman code exceeds its maximum length");
  }

 private:
  std::vector<HuffmanNode> nodes_;
};

std::pair<HuffmanTree, HuffmanTree> fixed_trees() {
  std::vector<unsigned> literal_lengths(288, 8);
  std::fill(literal_lengths.begin() + 144, literal_lengths.begin() + 256, 9);
  std::fill(literal_lengths.begin() + 256, literal_lengths.begin() + 280, 7);
  return {HuffmanTree(literal_lengths), HuffmanTree(std::vector<unsigned>(32, 5))};
}

std::pair<HuffmanTree, HuffmanTree> dynamic_trees(BitReader& reader) {
  const auto literal_count = reader.read_bits(5) + 257;
  const auto distance_count = reader.read_bits(5) + 1;
  const auto code_count = reader.read_bits(4) + 4;
  static constexpr std::array<unsigned, 19> order = {
      16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  std::vector<unsigned> code_lengths(19, 0);
  for (std::size_t i = 0; i < code_count; ++i)
    code_lengths[order[i]] = reader.read_bits(3);
  const HuffmanTree code_tree(code_lengths);

  std::vector<unsigned> lengths;
  lengths.reserve(literal_count + distance_count);
  while (lengths.size() < literal_count + distance_count) {
    const auto symbol = code_tree.decode(reader);
    if (symbol <= 15) {
      lengths.push_back(static_cast<unsigned>(symbol));
      continue;
    }
    unsigned repeat = 0;
    unsigned value = 0;
    if (symbol == 16) {
      if (lengths.empty())
        throw std::runtime_error("deflate repeat has no preceding length");
      repeat = reader.read_bits(2) + 3;
      value = lengths.back();
    } else if (symbol == 17) {
      repeat = reader.read_bits(3) + 3;
    } else if (symbol == 18) {
      repeat = reader.read_bits(7) + 11;
    } else {
      throw std::runtime_error("deflate code-length symbol is invalid");
    }
    if (repeat > literal_count + distance_count - lengths.size())
      throw std::runtime_error("deflate code-length repeat exceeds the tree");
    lengths.insert(lengths.end(), repeat, value);
  }
  std::vector<unsigned> literal_lengths(lengths.begin(),
                                        lengths.begin() + literal_count);
  if (literal_lengths.size() <= 256 || literal_lengths[256] == 0)
    throw std::runtime_error("deflate literal tree has no end marker");
  std::vector<unsigned> distance_lengths(lengths.begin() + literal_count,
                                         lengths.end());
  return {HuffmanTree(literal_lengths),
          HuffmanTree(distance_lengths, true)};
}

void inflate_codes(BitReader& reader, const HuffmanTree& literals,
                   const HuffmanTree& distances, std::string& output,
                   std::size_t max_output_bytes) {
  static constexpr std::array<unsigned, 29> length_base = {
      3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static constexpr std::array<unsigned, 29> length_extra = {
      0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static constexpr std::array<unsigned, 30> distance_base = {
      1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
      193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
      6145, 8193, 12289, 16385, 24577};
  static constexpr std::array<unsigned, 30> distance_extra = {
      0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

  while (true) {
    const auto symbol = literals.decode(reader);
    if (symbol < 256) {
      if (output.size() >= max_output_bytes)
        throw std::runtime_error("gzip output exceeds the configured limit");
      output.push_back(static_cast<char>(symbol));
      continue;
    }
    if (symbol == 256) return;
    if (symbol < 257 || symbol > 285)
      throw std::runtime_error("deflate length symbol is invalid");
    const auto length_index = static_cast<std::size_t>(symbol - 257);
    const auto length = length_base[length_index] +
                        reader.read_bits(length_extra[length_index]);
    const auto distance_symbol = distances.decode(reader);
    if (distance_symbol < 0 || distance_symbol >= 30)
      throw std::runtime_error("deflate distance symbol is invalid");
    const auto distance_index = static_cast<std::size_t>(distance_symbol);
    const auto distance = distance_base[distance_index] +
                          reader.read_bits(distance_extra[distance_index]);
    if (distance == 0 || distance > output.size())
      throw std::runtime_error("deflate back-reference distance is invalid");
    if (length > max_output_bytes - output.size())
      throw std::runtime_error("gzip output exceeds the configured limit");
    for (unsigned i = 0; i < length; ++i)
      output.push_back(output[output.size() - distance]);
  }
}

std::size_t inflate_member(std::string_view bytes, std::size_t offset,
                           std::string& output,
                           std::size_t max_output_bytes) {
  BitReader reader(bytes, offset);
  bool final = false;
  while (!final) {
    final = reader.read_bits(1) != 0;
    const auto type = reader.read_bits(2);
    if (type == 0) {
      reader.align_byte();
      const auto position = reader.byte_position();
      const auto length = read_u16(bytes, position);
      const auto inverse = read_u16(bytes, position + 2);
      if (static_cast<std::uint16_t>(length ^ 0xffffU) != inverse)
        throw std::runtime_error("deflate stored block length is invalid");
      if (position + 4 > bytes.size() ||
          length > bytes.size() - position - 4)
        throw std::runtime_error("deflate stored block is truncated");
      if (length > max_output_bytes - output.size())
        throw std::runtime_error("gzip output exceeds the configured limit");
      output.append(bytes.substr(position + 4, length));
      reader = BitReader(bytes, position + 4 + length);
    } else if (type == 1) {
      const auto [literals, distances] = fixed_trees();
      inflate_codes(reader, literals, distances, output, max_output_bytes);
    } else if (type == 2) {
      const auto [literals, distances] = dynamic_trees(reader);
      inflate_codes(reader, literals, distances, output, max_output_bytes);
    } else {
      throw std::runtime_error("deflate block type is reserved");
    }
  }
  reader.align_byte();
  return reader.byte_position();
}

std::size_t skip_zero_terminated(std::string_view bytes, std::size_t offset) {
  while (offset < bytes.size() && bytes[offset] != '\0') ++offset;
  if (offset == bytes.size()) throw std::runtime_error("gzip header is truncated");
  return offset + 1;
}

}  // namespace

bool is_gzip_data(std::string_view bytes) noexcept {
  return bytes.size() >= 2 &&
         static_cast<unsigned char>(bytes[0]) == 0x1fU &&
         static_cast<unsigned char>(bytes[1]) == 0x8bU;
}

#if defined(MINERA_HAS_ZLIB)
// Deflate at level 9 with zlib's default memory level, the settings Python's
// gzip module uses (and so pm4py's compressed XES export).
std::string gzip_compress(std::string_view bytes) {
  z_stream stream{};
  // windowBits 15 + 16 asks zlib for a gzip wrapper (header and CRC-32 trailer).
  if (deflateInit2(&stream, 9, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    throw std::runtime_error("gzip compressor could not be initialised");
  std::string output;
  std::array<unsigned char, 1U << 16U> buffer{};
  std::size_t offset = 0;
  int status = Z_OK;
  do {
    const auto chunk = std::min<std::size_t>(bytes.size() - offset,
                                             std::numeric_limits<uInt>::max());
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.data() + offset));
    stream.avail_in = static_cast<uInt>(chunk);
    offset += chunk;
    const int flush = offset == bytes.size() ? Z_FINISH : Z_NO_FLUSH;
    do {
      stream.next_out = buffer.data();
      stream.avail_out = static_cast<uInt>(buffer.size());
      status = deflate(&stream, flush);
      if (status == Z_STREAM_ERROR) {
        deflateEnd(&stream);
        throw std::runtime_error("gzip compression failed");
      }
      output.append(reinterpret_cast<const char*>(buffer.data()),
                    buffer.size() - stream.avail_out);
    } while (stream.avail_out == 0);
  } while (offset < bytes.size());
  deflateEnd(&stream);
  if (status != Z_STREAM_END) throw std::runtime_error("gzip compression did not finish");
  return output;
}
#else
// Without zlib: a valid gzip member of stored (uncompressed) deflate blocks.
std::string gzip_compress(std::string_view bytes) {
  std::string output;
  output.reserve(bytes.size() + bytes.size() / 65535 * 5 + 23);
  output.append("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10);
  std::size_t offset = 0;
  do {
    const auto remaining = bytes.size() - offset;
    const auto chunk = static_cast<std::uint16_t>(
        std::min<std::size_t>(remaining, 65535));
    const bool final = offset + chunk == bytes.size();
    output.push_back(final ? '\x01' : '\x00');
    append_u16(output, chunk);
    append_u16(output, static_cast<std::uint16_t>(chunk ^ 0xffffU));
    output.append(bytes.substr(offset, chunk));
    offset += chunk;
  } while (offset < bytes.size());
  append_u32(output, crc32(bytes));
  append_u32(output, static_cast<std::uint32_t>(bytes.size()));
  return output;
}
#endif

std::string gzip_decompress(std::string_view bytes,
                            std::size_t max_output_bytes) {
  if (!is_gzip_data(bytes)) throw std::runtime_error("input is not gzip data");
  std::string output;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 10 || !is_gzip_data(bytes.substr(offset)))
      throw std::runtime_error("gzip member header is invalid");
    if (static_cast<unsigned char>(bytes[offset + 2]) != 8U)
      throw std::runtime_error("gzip compression method is unsupported");
    const auto flags = static_cast<unsigned char>(bytes[offset + 3]);
    if ((flags & 0xe0U) != 0)
      throw std::runtime_error("gzip header uses reserved flags");
    std::size_t cursor = offset + 10;
    if ((flags & 0x04U) != 0) {
      const auto extra_length = read_u16(bytes, cursor);
      cursor += 2;
      if (cursor > bytes.size() || extra_length > bytes.size() - cursor)
        throw std::runtime_error("gzip extra header is truncated");
      cursor += extra_length;
    }
    if ((flags & 0x08U) != 0) cursor = skip_zero_terminated(bytes, cursor);
    if ((flags & 0x10U) != 0) cursor = skip_zero_terminated(bytes, cursor);
    if ((flags & 0x02U) != 0) {
      if (cursor > bytes.size() || bytes.size() - cursor < 2)
        throw std::runtime_error("gzip header checksum is truncated");
      cursor += 2;
    }

    const auto member_start = output.size();
    cursor = inflate_member(bytes, cursor, output, max_output_bytes);
    if (cursor > bytes.size() || bytes.size() - cursor < 8)
      throw std::runtime_error("gzip trailer is truncated");
    const auto expected_crc = read_u32(bytes, cursor);
    const auto expected_size = read_u32(bytes, cursor + 4);
    const std::string_view member(output.data() + member_start,
                                  output.size() - member_start);
    if (crc32(member) != expected_crc)
      throw std::runtime_error("gzip data checksum does not match");
    if (static_cast<std::uint32_t>(member.size()) != expected_size)
      throw std::runtime_error("gzip uncompressed size does not match");
    offset = cursor + 8;
  }
  return output;
}

}  // namespace minera::detail
