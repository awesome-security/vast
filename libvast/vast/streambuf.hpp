#ifndef VAST_STREAMBUF_HPP
#define VAST_STREAMBUF_HPP

#include <cstddef>
#include <streambuf>
#include <vector>

#include "vast/compression.hpp"

namespace vast {

/// A compressed streambuffer that compresses/uncompresses from an underlying
/// `std::streambuf`. This streambuffer writes/reads blocks of data in the
/// following format:
///
///     +-------------------+-----------------+--------------------...---+
///     | uncompressed size | compressed size |  compressed data         |
///     +-------------------+-----------------+--------------------...---+
///
/// Both sizes are written in *variable byte* encoding to save space.
class compressedbuf : public std::streambuf {
public:
  /// The default buffer size in bytes.
  static constexpr size_t default_block_size = 16 << 10;

  /// Constructs an compressed streambuffer.
  /// @param sb The underlying streambuffer to read from or write to.
  /// @param method The compression method to use for each block.
  /// @param block_size The size of the internal buffer for uncompressed data.
  /// @pre `block_size > 1`
  compressedbuf(std::streambuf& sb,
                compression method = compression::null,
                size_t block_size = default_block_size);

  /// Destroys the streambuffer after synchronizing it with the underlying
  /// stream by calling sync().
  ~compressedbuf();

protected:
  // -- buffer management and positioning ------------------------------------

  /// If a put area exists, calls `overflow()` to write all pending output to
  /// the underlying streambuffer, then clears its internal buffers.
  /// @returns -1 on failure or the number of characters written to the
  ///          underlying streambuffer otherwise.
  int sync() override;

  // -- put area -------------------------------------------------------------

  int_type overflow(int_type c) override;

  std::streamsize xsputn(char_type const* s, std::streamsize n) override;

  // -- get area -------------------------------------------------------------

  int_type underflow() override;

  std::streamsize xsgetn(char_type* s, std::streamsize n) override;

private:
  void compress();
  void uncompress();

  std::streambuf& streambuf_;
  compression method_;
  size_t block_size_;
  std::vector<char> compressed_;
  std::vector<char> uncompressed_;
};

} // namespace vast

#endif
