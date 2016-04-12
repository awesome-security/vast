#include "vast/save.hpp"
#include "vast/load.hpp"

#include "test.hpp"

using namespace std::string_literals;
using namespace vast;

TEST(save/load) {
  std::string buf;
  auto m = save<compression::lz4>(buf, 42, 4.2, 1337u, "foo"s);
  CHECK(!m.error());
  int i;
  double d;
  unsigned u;
  std::string s;
  m = load<compression::lz4>(buf, i, d, u, s);
  CHECK(!m.error());
  CHECK_EQUAL(i, 42);
  CHECK_EQUAL(d, 4.2);
  CHECK_EQUAL(u, 1337u);
  CHECK_EQUAL(s, "foo");
}
