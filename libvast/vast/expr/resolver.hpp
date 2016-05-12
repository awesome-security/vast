#ifndef VAST_EXPR_RESOLVER_HPP
#define VAST_EXPR_RESOLVER_HPP

#include "vast/expression.hpp"
#include "vast/maybe.hpp"
#include "vast/schema.hpp"

namespace vast {
namespace expr {

/// Transforms schema extractors into one or more data extractors.
struct schema_resolver {
  schema_resolver(type const& t);

  maybe<expression> operator()(none);
  maybe<expression> operator()(conjunction const& c);
  maybe<expression> operator()(disjunction const& d);
  maybe<expression> operator()(negation const& n);
  maybe<expression> operator()(predicate const& p);
  maybe<expression> operator()(schema_extractor const& e, data const& d);
  maybe<expression> operator()(data const& d, schema_extractor const& e);

  template <typename T, typename U>
  maybe<expression> operator()(T const& lhs, U const& rhs) {
    return {predicate{lhs, op_, rhs}};
  }

  relational_operator op_;
  type const& type_;
};

// Resolves type and data extractor predicates. Specifically, it handles the
// following predicates:
// - Type extractor: replaces the predicate with one or more data extractors.
// - Data extractor: removes the predicate if the event type does not match the
//   type given to this visitor.
struct type_resolver {
  type_resolver(type const& event_type);

  expression operator()(none);
  expression operator()(conjunction const& c);
  expression operator()(disjunction const& d);
  expression operator()(negation const& n);
  expression operator()(predicate const& p);

  relational_operator op_;
  type const& type_;
};

} // namespace expr
} // namespace vast

#endif
