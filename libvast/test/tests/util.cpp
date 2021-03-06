#include "vast/error.hpp"
#include "vast/trial.hpp"
#include "vast/result.hpp"
#include "vast/util/flat_serial_set.hpp"

#define SUITE util
#include "test.hpp"

using namespace vast;

TEST(error) {
  error e;
  error shoot{"holy cow"};
  CHECK(shoot.msg() == "holy cow");
}

TEST(trial) {
  trial<int> t = 42;
  REQUIRE(t);
  CHECK(*t == 42);

  trial<int> u = std::move(t);
  REQUIRE(u);
  CHECK(*u == 42);

  t = error{"whoops"};
  CHECK(!t);

  t = std::move(u);
  CHECK(t);

  trial<void> x;
  CHECK(x);
  x = error{"bad"};
  CHECK(!x);
  x = nothing;
  CHECK(x);
}

TEST(result) {
  result<int> t;
  REQUIRE(t.empty());
  REQUIRE(!t.engaged());
  REQUIRE(!t.failed());

  t = 42;
  REQUIRE(!t.empty());
  REQUIRE(t.engaged());
  REQUIRE(!t.failed());
  REQUIRE(*t == 42);

  t = error{"whoops"};
  REQUIRE(!t.empty());
  REQUIRE(!t.engaged());
  REQUIRE(t.failed());

  CHECK(t.error().msg() == "whoops");
}

TEST(flat_serial_set) {
  util::flat_serial_set<int> set;
  // Insert elements.
  CHECK(set.push_back(1));
  CHECK(set.push_back(2));
  CHECK(set.push_back(3));
  // Ensure no duplicates.
  CHECK(!set.push_back(2));
  // Test membership.
  CHECK(set[0] == 1);
  CHECK(set[2] == 3);
}
