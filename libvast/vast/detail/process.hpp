#ifndef VAST_DETAIL_PROCESS_HPP
#define VAST_DETAIL_PROCESS_HPP

#include <utility>

namespace vast {
namespace detail {

template <class Processor, class T>
void process(Processor& proc, T&& x) {
  proc & const_cast<T&>(x); // CAF promises not to touch it.
}

template <class Processor, class T, class... Ts>
void process(Processor& proc, T&& x, Ts&&... xs) {
  process(proc, std::forward<T>(x));
  process(proc, std::forward<Ts>(xs)...);
}

} // namespace detail
} // namespace vast

#endif
