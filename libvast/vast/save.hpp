#ifndef VAST_SAVE_HPP
#define VAST_SAVE_HPP

#include <fstream>
#include <streambuf>
#include <type_traits>

#include <caf/stream_serializer.hpp>
#include <caf/stream_deserializer.hpp>

#include "vast/detail/process.hpp"
#include "vast/detail/type_traits.hpp"

#include "vast/compression.hpp"
#include "vast/maybe.hpp"
#include "vast/filesystem.hpp"
#include "vast/streambuf.hpp"

namespace vast {

/// Serializes a sequence of objects into a streambuffer.
/// @see load
template <
  compression Method = compression::null,
  class Streambuf,
  class T,
  class... Ts
>
auto save(Streambuf& streambuf, T&& x, Ts&&... xs)
-> std::enable_if_t<detail::is_streambuf<Streambuf>::value, maybe<void>> {
  // A good optimizer clears that branch.
  if (Method == compression::null) {
    caf::stream_serializer<Streambuf&> s{streambuf};
    detail::process(s, std::forward<T>(x), std::forward<Ts>(xs)...);
  } else {
    compressedbuf compressed{streambuf, Method};
    caf::stream_serializer<compressedbuf&> s{compressed};
    detail::process(s, std::forward<T>(x), std::forward<Ts>(xs)...);
  }
  return {};
}

/// Serializes a sequence of objects into a container of bytes.
/// @see load
template <
  compression Method = compression::null,
  class Container,
  class T,
  class... Ts
>
auto save(Container& container, T&& x, Ts&&... xs)
-> std::enable_if_t<
  detail::is_contiguous_byte_container<Container>::value,
  maybe<void>
> {
  caf::containerbuf<Container> sink{container};
  return save<Method>(sink, std::forward<T>(x), std::forward<Ts>(xs)...);
}

/// Serializes a sequence of objects into a file.
/// @see load
template <
  compression Method = compression::null,
  class T,
  class... Ts
>
maybe<void> save(path const& p, T&& x, Ts&&... xs) {
  std::ofstream fs{p.str()};
  return save<Method>(*fs.rdbuf(), std::forward<T>(x), std::forward<Ts>(xs)...);
}

} // namespace vast

#endif
