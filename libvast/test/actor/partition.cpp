#include "vast/bitstream.hpp"
#include "vast/event.hpp"
#include "vast/actor/partition.hpp"
#include "vast/actor/task.hpp"
#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/expression.hpp"

#define SUITE actors
#include "test.hpp"
#include "fixtures/events.hpp"

using namespace vast;

FIXTURE_SCOPE(fixture_scope, fixtures::simple_events)

TEST(partition) {
  using bitstream_type = partition::bitstream_type;

  MESSAGE("sending events to partition");
  path dir = "vast-test-partition";
  scoped_actor self;
  auto p = self->spawn<monitored + priority_aware>(partition::make, dir, self);
  auto t = self->spawn<monitored>(task::make<time::moment, uint64_t>,
                                  time::snapshot(), events0.size());
  self->send(p, events0, sch, t);
  self->receive([&](down_msg const& msg) { CHECK(msg.source == t); });
  t = self->spawn<monitored>(task::make<time::moment, uint64_t>,
                             time::snapshot(), events1.size());
  self->send(p, events1, sch, t);
  self->receive([&](down_msg const& msg) { CHECK(msg.source == t); });

  MESSAGE("flushing partition through termination");
  self->send_exit(p, exit::done);
  self->receive([&](down_msg const& msg) { CHECK(msg.source == p); });

  MESSAGE("reloading partition and running a query against it");
  p = self->spawn<monitored + priority_aware>(partition::make, dir, self);
  auto expr = to<expression>("&time < now && c >= 42 && c < 84");
  REQUIRE(expr);
  self->send(p, *expr, historical_atom::value);
  bool done = false;
  bitstream_type hits;
  self->do_receive(
    [&](expression const& e, bitstream_type const& h, historical_atom) {
      CHECK(*expr == e);
      hits |= h;
    },
    [&](done_atom, time::moment, expression const& e) {
      CHECK(*expr == e);
      done = true;
    }
  ).until([&] { return done; });
  CHECK(hits.count() == 42);

  MESSAGE("creating a continuous query");
  expr = to<expression>("s ni \"7\"");
  REQUIRE(expr);
  self->send(p, *expr, continuous_atom::value);

  MESSAGE("sending another event");
  t = self->spawn<monitored>(task::make<time::moment, uint64_t>,
                             time::snapshot(), events.size());
  self->send(p, events, sch, t);
  self->receive([&](down_msg const& msg) { CHECK(msg.source == t); });

  MESSAGE("getting continuous hits");
  self->receive(
    [&](expression const& e, bitstream_type const& hits, continuous_atom) {
      CHECK(*expr == e);
      // (0..1024)
      //   .select{|x| x % 2 == 0}
      //   .map{|x| x.to_s}
      //   .select{|x| x =~ /7/}
      //   .length == 95
      CHECK(hits.count() == 95);
    });

  MESSAGE("disabling continuous query and sending another event");
  self->send(p, *expr, continuous_atom::value, disable_atom::value);
  auto e = event::make(record{1337u, std::to_string(1337)}, type0);
  e.id(4711);
  t = self->spawn<monitored>(task::make<time::moment, uint64_t>,
                             time::snapshot(), 1);
  self->send(p, std::vector<event>{std::move(e)}, sch, t);
  self->receive([&](down_msg const& msg) { CHECK(msg.source == t); });
  // Make sure that we didn't get any new hits.
  CHECK(self->mailbox().count() == 0);

  MESSAGE("cleaning up");
  self->send_exit(p, exit::done);
  self->await_all_other_actors_done();
  rm(dir);
}

FIXTURE_SCOPE_END()
