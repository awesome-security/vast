#ifndef VAST_INDIVIDUAL_HPP
#define VAST_INDIVIDUAL_HPP

#include "vast/uuid.hpp"
#include "vast/util/operators.hpp"

namespace vast {

/// An object with a unique ID.
class individual : util::totally_ordered<individual> {
  friend access;

public:
  /// Constructs an object with a random ID.
  /// @param id The instance's UUID.
  individual(uuid id = uuid::random());

  friend bool operator<(individual const& x, individual const& y);
  friend bool operator==(individual const& x, individual const& y);

  /// Retrieves the individual's ID.
  /// @returns A UUID describing the instance.
  uuid const& id() const;

  /// Sets the individual's ID.
  /// @param id A UUID describing the instance.
  void id(uuid id);

private:
  uuid id_;
};

} // namespace vast

#endif
