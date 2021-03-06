#ifndef VAST_CONCEPT_PARSEABLE_CORE_SEQUENCE_CHOICE_HPP
#define VAST_CONCEPT_PARSEABLE_CORE_SEQUENCE_CHOICE_HPP

#include <tuple>
#include <type_traits>

#include "vast/concept/parseable/core/parser.hpp"
#include "vast/concept/parseable/core/optional.hpp"

namespace vast {

// (LHS >> ~RHS) | RHS
template <typename Lhs, typename Rhs>
class sequence_choice_parser : public parser<sequence_choice_parser<Lhs, Rhs>> {
public:
  using lhs_type = Lhs;
  using rhs_type = Rhs;
  using lhs_attribute = typename Lhs::attribute;
  using rhs_attribute = typename Rhs::attribute;

  // LHS = unused && RHS = unused  =>  unused
  // LHS = T && RHS = unused       =>  maybe<LHS>
  // LHS = unused && RHS = T       =>  maybe<RHS>
  // LHS = T && RHS = U            =>  std:tuple<maybe<LHS>, maybe<RHS>>
  using attribute =
    std::conditional_t<
      std::is_same<lhs_attribute, unused_type>{}
        && std::is_same<rhs_attribute, unused_type>{},
      unused_type,
      std::conditional_t<
        std::is_same<rhs_attribute, unused_type>{},
        maybe<lhs_attribute>,
        std::conditional_t<
          std::is_same<lhs_attribute, unused_type>{},
          maybe<rhs_attribute>,
          std::tuple<maybe<lhs_attribute>, maybe<rhs_attribute>>
        >
      >
    >;

  sequence_choice_parser(Lhs lhs, Rhs rhs)
    : lhs_{std::move(lhs)}, rhs_{rhs}, rhs_opt_{std::move(rhs)} {
  }

  template <typename Iterator, typename Attribute>
  bool parse(Iterator& f, Iterator const& l, Attribute& a) const {
    maybe<rhs_attribute> rhs_attr;
    if (lhs_.parse(f, l, left_attr(a)) && rhs_opt_.parse(f, l, rhs_attr)) {
      right_attr(a) = std::move(rhs_attr);
      return true;
    }
    return rhs_.parse(f, l, right_attr(a));
  }

private:
  template <
    typename Attribute,
    typename L = lhs_attribute,
    typename R = rhs_attribute
  >
  static auto left_attr(Attribute&)
    -> std::enable_if_t<std::is_same<L, unused_type>{}, unused_type&> {
    return unused;
  }

  template <
    typename Attribute,
    typename L = lhs_attribute,
    typename R = rhs_attribute
  >
  static auto left_attr(Attribute& a)
    -> std::enable_if_t<
         ! std::is_same<L, unused_type>{} && std::is_same<R, unused_type>{},
         maybe<L>&
       > {
    return a;
  }

  template <
    typename... Ts,
    typename L = lhs_attribute,
    typename R = rhs_attribute
  >
  static auto left_attr(std::tuple<Ts...>& t)
    -> std::enable_if_t<
         ! std::is_same<L, unused_type>{} && ! std::is_same<R, unused_type>{},
         maybe<L>&
       > {
    return std::get<0>(t);
  }

  template <
    typename Attribute,
    typename L = lhs_attribute,
    typename R = rhs_attribute
  >
  static auto right_attr(Attribute&)
    -> std::enable_if_t<std::is_same<R, unused_type>{}, unused_type&> {
    return unused;
  }

  template <
    typename Attribute,
    typename L = lhs_attribute,
    typename R = rhs_attribute
  >
  static auto right_attr(Attribute& a)
    -> std::enable_if_t<
         std::is_same<L, unused_type>{} && ! std::is_same<R, unused_type>{},
         maybe<R>&
       > {
    return a;
  }

  template <
    typename... Ts,
    typename L = lhs_attribute,
    typename R = rhs_attribute
  >
  static auto right_attr(std::tuple<Ts...>& t)
    -> std::enable_if_t<
         ! std::is_same<L, unused_type>{} && ! std::is_same<R, unused_type>{},
         maybe<R>&
       > {
    return std::get<1>(t);
  }

  lhs_type lhs_;
  rhs_type rhs_;
  optional_parser<rhs_type> rhs_opt_;
};

} // namespace vast

#endif
