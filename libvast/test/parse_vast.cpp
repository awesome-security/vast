#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/address.hpp"
#include "vast/concept/parseable/vast/endpoint.hpp"
#include "vast/concept/parseable/vast/http.hpp"
#include "vast/concept/parseable/vast/key.hpp"
#include "vast/concept/parseable/vast/offset.hpp"
#include "vast/concept/parseable/vast/pattern.hpp"
#include "vast/concept/parseable/vast/port.hpp"
#include "vast/concept/parseable/vast/subnet.hpp"
#include "vast/concept/parseable/vast/time.hpp"
#include "vast/concept/parseable/vast/uri.hpp"
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/address.hpp"
#include "vast/concept/printable/vast/pattern.hpp"
#include "vast/concept/printable/vast/time.hpp"

#define SUITE parseable
#include "test.hpp"

using namespace vast;
using namespace std::string_literals;

TEST(time::duration) {
  time::duration d;

  MESSAGE("nanoseconds");
  CHECK(parsers::time_duration("42 nsecs", d));
  CHECK(d == time::nanoseconds(42));
  CHECK(parsers::time_duration("43nsecs", d));
  CHECK(d == time::nanoseconds(43));
  CHECK(parsers::time_duration("44ns", d));
  CHECK(d == time::nanoseconds(44));

  MESSAGE("microseconds");
  CHECK(parsers::time_duration("42 usecs", d));
  CHECK(d == time::microseconds(42));
  CHECK(parsers::time_duration("43usecs", d));
  CHECK(d == time::microseconds(43));
  CHECK(parsers::time_duration("44us", d));
  CHECK(d == time::microseconds(44));

  MESSAGE("milliseconds");
  CHECK(parsers::time_duration("42 msecs", d));
  CHECK(d == time::milliseconds(42));
  CHECK(parsers::time_duration("43msecs", d));
  CHECK(d == time::milliseconds(43));
  CHECK(parsers::time_duration("44ms", d));
  CHECK(d == time::milliseconds(44));

  MESSAGE("seconds");
  CHECK(parsers::time_duration("-42 secs", d));
  CHECK(d == time::seconds(-42));
  CHECK(parsers::time_duration("-43secs", d));
  CHECK(d == time::seconds(-43));
  CHECK(parsers::time_duration("-44s", d));
  CHECK(d == time::seconds(-44));

  MESSAGE("minutes");
  CHECK(parsers::time_duration("-42 mins", d));
  CHECK(d == time::minutes(-42));
  CHECK(parsers::time_duration("-43min", d));
  CHECK(d == time::minutes(-43));
  CHECK(parsers::time_duration("44m", d));
  CHECK(d == time::minutes(44));

  MESSAGE("hours");
  CHECK(parsers::time_duration("42 hours", d));
  CHECK(d == time::hours(42));
  CHECK(parsers::time_duration("-43hrs", d));
  CHECK(d == time::hours(-43));
  CHECK(parsers::time_duration("44h", d));
  CHECK(d == time::hours(44));

  // TODO
  // MESSAGE("compound");
  // CHECK(parsers::time_duration("5m99s", d));
  // CHECK(d.count() == 399000000000ll);
}

TEST(time::point) {
  time::point tp;
  MESSAGE("YYYY-MM-DD+HH:MM:SS");
  CHECK(parsers::time_point("2012-08-12+23:55:04", tp));
  CHECK(tp == time::point::utc(2012, 8, 12, 23, 55, 4));
  MESSAGE("YYYY-MM-DD+HH:MM");
  CHECK(parsers::time_point("2012-08-12+23:55", tp));
  CHECK(tp == time::point::utc(2012, 8, 12, 23, 55));
  MESSAGE("YYYY-MM-DD+HH");
  CHECK(parsers::time_point("2012-08-12+23", tp));
  CHECK(tp == time::point::utc(2012, 8, 12, 23));
  MESSAGE("YYYY-MM-DD");
  CHECK(parsers::time_point("2012-08-12", tp));
  CHECK(tp == time::point::utc(2012, 8, 12));
  MESSAGE("YYYY-MM");
  CHECK(parsers::time_point("2012-08", tp));
  CHECK(tp == time::point::utc(2012, 8));
  MESSAGE("UNIX epoch");
  CHECK(parsers::time_point("@1444040673", tp));
  CHECK(tp.time_since_epoch() == time::seconds{1444040673});
  CHECK(parsers::time_point("@1398933902.686337", tp));
  CHECK(tp.time_since_epoch() == time::double_seconds{1398933902.686337});
  MESSAGE("now");
  CHECK(parsers::time_point("now", tp));
  CHECK(tp > time::now() - time::minutes{1});
  CHECK(tp < time::now() + time::minutes{1});
  CHECK(parsers::time_point("now - 1m", tp));
  CHECK(tp < time::now());
  CHECK(parsers::time_point("now + 1m", tp));
  CHECK(tp > time::now());
  MESSAGE("ago");
  CHECK(parsers::time_point("10 days ago", tp));
  CHECK(tp < time::now());
  MESSAGE("in");
  CHECK(parsers::time_point("in 1 year", tp));
  CHECK(tp > time::now());
}

TEST(pattern) {
  auto p = make_parser<pattern>{};
  auto str = "/^\\w{3}\\w{3}\\w{3}$/"s;
  auto f = str.begin();
  auto l = str.end();
  pattern pat;
  CHECK(p.parse(f, l, pat));
  CHECK(f == l);
  CHECK(to_string(pat) == str);

  str = "/foo\\+(bar){2}|\"baz\"*/";
  pat = {};
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, pat));
  CHECK(f == l);
  CHECK(to_string(pat) == str);
}

TEST(address) {
  auto p = make_parser<address>{};

  MESSAGE("IPv4");
  auto str = "192.168.0.1"s;
  auto f = str.begin();
  auto l = str.end();
  address a;
  CHECK(p.parse(f, l, a));
  CHECK(f == l);
  CHECK(a.is_v4());
  CHECK(to_string(a) == str);

  MESSAGE("IPv6");
  str = "::";
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, a));
  CHECK(f == l);
  CHECK(a.is_v6());
  CHECK(to_string(a) == str);
  str = "beef::cafe";
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, a));
  CHECK(f == l);
  CHECK(a.is_v6());
  CHECK(to_string(a) == str);
  str = "f00::cafe";
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, a));
  CHECK(f == l);
  CHECK(a.is_v6());
  CHECK(to_string(a) == str);
}

TEST(subnet) {
  auto p = make_parser<subnet>{};

  MESSAGE("IPv4");
  auto str = "192.168.0.0/24"s;
  auto f = str.begin();
  auto l = str.end();
  subnet s;
  CHECK(p.parse(f, l, s));
  CHECK(f == l);
  CHECK(s == subnet{*to<address>("192.168.0.0"), 24});
  CHECK(s.network().is_v4());

  MESSAGE("IPv6");
  str = "beef::cafe/40";
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, s));
  CHECK(f == l);
  CHECK(s == subnet{*to<address>("beef::cafe"), 40});
  CHECK(s.network().is_v6());
}

TEST(port) {
  auto p = make_parser<port>();

  MESSAGE("tcp");
  auto str = "22/tcp"s;
  auto f = str.begin();
  auto l = str.end();
  port prt;
  CHECK(p.parse(f, l, prt));
  CHECK(f == l);
  CHECK(prt == port{22, port::tcp});

  MESSAGE("udp");
  str = "53/udp"s;
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, prt));
  CHECK(f == l);
  CHECK(prt == port{53, port::udp});

  MESSAGE("icmp");
  str = "7/icmp"s;
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, prt));
  CHECK(f == l);
  CHECK(prt == port{7, port::icmp});

  MESSAGE("unknown");
  str = "42/?"s;
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, prt));
  CHECK(f == l);
  CHECK(prt == port{42, port::unknown});
}

TEST(key) {
  key k;
  CHECK(parsers::key("foo.bar_baz.qux", k));
  CHECK(k == key{"foo", "bar_baz", "qux"});
}

TEST(offset) {
  offset o;
  CHECK(parsers::offset("1,2,3", o));
  CHECK(o == offset{1, 2, 3});
}

TEST(HTTP header) {
  auto p = make_parser<http::header>();
  auto str = "foo: bar"s;
  auto f = str.begin();
  auto l = str.end();
  http::header hdr;
  CHECK(p.parse(f, l, hdr));
  CHECK(hdr.name == "FOO");
  CHECK(hdr.value == "bar");
  CHECK(f == l);

  str = "Content-Type:application/pdf";
  f = str.begin();
  l = str.end();
  CHECK(p.parse(f, l, hdr));
  CHECK(hdr.name == "CONTENT-TYPE");
  CHECK(hdr.value == "application/pdf");
  CHECK(f == l);
}

TEST(HTTP request) {
  auto p = make_parser<http::request>();
  auto str = "GET /foo/bar%20baz/ HTTP/1.1\r\n"
             "Content-Type:text/html\r\n"
             "Content-Length:1234\r\n"
             "\r\n"
             "Body "s;
  auto f = str.begin();
  auto l = str.end();
  http::request req;
  CHECK(p.parse(f, l, req));
  CHECK(req.method == "GET");
  CHECK(req.uri.path[0] == "foo");
  CHECK(req.uri.path[1] == "bar baz");
  CHECK(req.protocol == "HTTP");
  CHECK(req.version == 1.1);
  auto hdr = req.header("content-type");
  REQUIRE(hdr);
  CHECK(hdr->name == "CONTENT-TYPE");
  CHECK(hdr->value == "text/html");
  hdr = req.header("content-length");
  REQUIRE(hdr);
  CHECK(hdr->name == "CONTENT-LENGTH");
  CHECK(hdr->value == "1234");
  CHECK(f == l);
}

TEST(URI with HTTP URL) {
  auto p = make_parser<uri>();
  auto str = "http://foo.bar:80/foo/bar?opt1=val1&opt2=x+y#frag1"s;
  auto f = str.begin();
  auto l = str.end();
  uri u;
  CHECK(p.parse(f, l, u));
  CHECK(u.scheme == "http");
  CHECK(u.host == "foo.bar");
  CHECK(u.port == 80);
  CHECK(u.path[0] == "foo");
  CHECK(u.path[1] == "bar");
  CHECK(u.query["opt1"] == "val1");
  CHECK(u.query["opt2"] == "x y");
  CHECK(u.fragment == "frag1");
  CHECK(f == l);
}

TEST(URI with path only) {
  auto p = make_parser<uri>();
  auto str = "/foo/bar?opt1=val1&opt2=val2"s;
  auto f = str.begin();
  auto l = str.end();
  uri u;
  CHECK(p.parse(f, l, u));
  CHECK(u.scheme == "");
  CHECK(u.host == "");
  CHECK(u.port == 0);
  CHECK(u.path[0] == "foo");
  CHECK(u.path[1] == "bar");
  CHECK(u.query["opt1"] == "val1");
  CHECK(u.query["opt2"] == "val2");
  CHECK(u.fragment == "");
  CHECK(f == l);
}

TEST(endpoint) {
  endpoint e;
  CHECK(parsers::endpoint(":42000", e));
  CHECK(e.host == "");
  CHECK(e.port == 42000);
  CHECK(parsers::endpoint("localhost", e));
  CHECK(e.host == "localhost");
  CHECK(e.port == 0);
  CHECK(parsers::endpoint("10.0.0.1:80", e));
  CHECK(e.host == "10.0.0.1");
  CHECK(e.port == 80);
  CHECK(parsers::endpoint("foo-bar_baz.test", e));
  CHECK(e.host == "foo-bar_baz.test");
  CHECK(e.port == 0);
}
