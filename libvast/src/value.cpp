#include "vast/value.hpp"
#include "vast/util/assert.hpp"

namespace vast {

bool value::type(vast::type const& t) {
  if (!t.check(data_))
    return false;
  type_ = t;
  return true;
}

type const& value::type() const {
  return type_;
}

vast::data const& value::data() const {
  return data_;
}

data::variant_type& expose(value& v) {
  return expose(v.data_);
}

data::variant_type const& expose(value const& v) {
  return expose(v.data_);
}

bool operator==(value const& lhs, value const& rhs) {
  return lhs.data_ == rhs.data_;
}

bool operator!=(value const& lhs, value const& rhs) {
  return lhs.data_ != rhs.data_;
}

bool operator<(value const& lhs, value const& rhs) {
  return lhs.data_ < rhs.data_;
}

bool operator<=(value const& lhs, value const& rhs) {
  return lhs.data_ <= rhs.data_;
}

bool operator>=(value const& lhs, value const& rhs) {
  return lhs.data_ >= rhs.data_;
}

bool operator>(value const& lhs, value const& rhs) {
  return lhs.data_ > rhs.data_;
}

value flatten(value const& v) {
  return {flatten(v.data()), flatten(v.type())};
}

} // namespace vast
