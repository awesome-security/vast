#ifndef VAST_CONCEPT_SERIALIZABLE_STATE_HPP
#define VAST_CONCEPT_SERIALIZABLE_STATE_HPP

#include <type_traits>

#include "vast/access.hpp"

namespace caf {
class deserializer;
class serializer;
}

namespace vast {
namespace detail {

// Variadic helpers to interface with CAF's serialization framework.

template <class Processor, class T>
void process(Processor& proc, T& x) {
  proc & x;
}

template <class Processor, class T, class... Ts>
void process(Processor& proc, T& x, Ts&... xs) {
  process(proc, x);
  process(proc, xs...);
}

template <class T>
void save(caf::serializer& sink, T const& x) {
  sink << x;
}

template <class T, class... Ts>
void save(caf::serializer& sink, T const& x, Ts const&... xs) {
  save(sink, x);
  save(sink, xs...);
}

template <class T>
void load(caf::deserializer& source, T& x) {
  source >> x;
}

template <class T, class... Ts>
void load(caf::deserializer& source, T& x, Ts&... xs) {
  load(source, x);
  load(source, xs...);
}

// Dummy function that faciliates expression SFINAE below.
struct dummy {
  template <class...>
  void operator()(...);
};

} // namespace detail

// Generic implementation of CAF's free serialization functions for all types
// in namespace 'vast'.

template <class Processor, class T>
auto serialize(Processor& proc, T& x)
-> decltype(access::state<T>::call(x, std::declval<detail::dummy>())) {
  auto f = [&](auto&... xs) { detail::process(proc, xs...); };
  access::state<T>::call(x, f);
}

template <class T>
auto serialize(caf::serializer& sink, T const& x)
-> decltype(access::state<T>::read(x, std::declval<detail::dummy>())) {
  auto f = [&](auto&... xs) { detail::save(sink, xs...); };
  access::state<T>::read(x, f);
}

template <class T>
auto serialize(caf::deserializer& source, T& x)
-> decltype(access::state<T>::read(x, std::declval<detail::dummy>())) {
  auto f = [&](auto&... xs) { detail::load(source, xs...); };
  access::state<T>::write(x, f);
}

// Extend this concept to namespace 'time', because CAF finds the serialize
// overloads via ADL.
namespace time {
using vast::serialize;
}

} // namespace vast

#endif
