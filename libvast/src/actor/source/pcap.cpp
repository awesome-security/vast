#include <netinet/in.h>

#include <thread>

#include "vast/event.hpp"
#include "vast/filesystem.hpp"
#include "vast/actor/source/pcap.hpp"
#include "vast/detail/pcap_packet_type.hpp"
#include "vast/util/assert.hpp"
#include "vast/util/byte_swap.hpp"

namespace vast {
namespace source {

pcap_state::pcap_state(local_actor* self)
  : state{self, "pcap-source"},
    packet_type_{vast::detail::pcap_packet_type},
    generator_{std::random_device{}()} {
}

pcap_state::~pcap_state() {
  if (pcap_)
    ::pcap_close(pcap_);
}

schema pcap_state::schema() {
  vast::schema sch;
  sch.add(packet_type_);
  return sch;
}

void pcap_state::schema(vast::schema const& sch) {
  auto t = sch.find("vast::packet");
  if (!t) {
    VAST_ERROR_AT(self, "did not find type vast::packet in given schema");
    return;
  }
  if (!congruent(packet_type_, *t)) {
    VAST_WARN_AT(self, "ignores incongruent schema type:", t->name());
    return;
  }
  VAST_VERBOSE_AT(self, "prefers type in schema over default type");
  packet_type_ = *t;
}

result<event> pcap_state::extract() {
  char buf[PCAP_ERRBUF_SIZE]; // for errors.
  if (!pcap_ && !done_) {
    // Determine interfaces.
    pcap_if_t* iface;
    if (::pcap_findalldevs(&iface, buf) == -1)
      return error{"failed to enumerate interfaces: ", buf};
    for (auto i = iface; i != nullptr; i = i->next)
      if (input_ == i->name) {
        pcap_ = ::pcap_open_live(i->name, 65535, 1, 1000, buf);
        if (!pcap_) {
          ::pcap_freealldevs(iface);
          return error{"failed to open interface ", input_, ": ", buf};
        }
        if (pseudo_realtime_ > 0) {
          pseudo_realtime_ = 0;
          VAST_WARN_AT(self, "ignores pseudo-realtime in live mode");
        }
        VAST_INFO_AT(self, "listens on interface " << i->name);
        break;
      }
    ::pcap_freealldevs(iface);
    if (!pcap_) {
      if (input_ != "-" && !exists(input_))
        return error{"no such file: ", input_};
#ifdef PCAP_TSTAMP_PRECISION_NANO
      pcap_ = ::pcap_open_offline_with_tstamp_precision(
        input_.c_str(), PCAP_TSTAMP_PRECISION_NANO, buf);
#else
      pcap_ = ::pcap_open_offline(input_.c_str(), buf);
#endif
      if (!pcap_) {
        std::string err{buf};
        flows_.clear();
        return error{"failed to open pcap file ", input_, ": ", err};
      }

      VAST_INFO_AT(self, "reads trace from", input_);
      if (pseudo_realtime_ > 0)
        VAST_INFO_AT(self, "uses pseudo-realtime factor 1/" <<
                     pseudo_realtime_);
    }
    VAST_VERBOSE_AT(self, "cuts off flows after", cutoff_,
                    "bytes in each direction");
    VAST_VERBOSE_AT(self, "keeps at most", max_flows_, "concurrent flows");
    VAST_VERBOSE_AT(self, "evicts flows after", max_age_ << "s of inactivity");
    VAST_VERBOSE_AT(self, "expires flow table every", expire_interval_ << "s");
  }
  uint8_t const* data;
  auto r = ::pcap_next_ex(pcap_, &packet_header_, &data);
  if (r == 0)
    return {}; // Attempt to fetch next packet timed out.
  if (r == -2) {
    done_ = true;
    return {}; // Reached end of trace.
  }
  if (r == -1) {
    std::string err{::pcap_geterr(pcap_)};
    pcap_ = nullptr;
    done_ = true;
    return error{"failed to get next packet: ", err};
  }
  // Parse packet.
  detail::connection conn;
  auto packet_size = packet_header_->len - 14;
  auto layer3 = data + 14;
  uint8_t const* layer4 = nullptr;
  uint8_t layer4_proto = 0;
  auto layer2_type = *reinterpret_cast<uint16_t const*>(data + 12);
  uint64_t payload_size = packet_size;
  switch (util::byte_swap<network_endian, host_endian>(layer2_type)) {
    default:
      return {}; // Skip all non-IP packets.
    case 0x0800: {
      if (packet_header_->len < 14 + 20)
        return error{"IPv4 header too short"};
      size_t header_size = (*layer3 & 0x0f) * 4;
      if (header_size < 20)
        return error{"IPv4 header too short: ", header_size, " bytes"};
      auto orig_h = reinterpret_cast<uint32_t const*>(layer3 + 12);
      auto resp_h = reinterpret_cast<uint32_t const*>(layer3 + 16);
      conn.src = {orig_h, address::ipv4, address::network};
      conn.dst = {resp_h, address::ipv4, address::network};
      layer4_proto = *(layer3 + 9);
      layer4 = layer3 + header_size;
      payload_size -= header_size;
    } break;
    case 0x86dd: {
      if (packet_header_->len < 14 + 40)
        return error{"IPv6 header too short"};
      auto orig_h = reinterpret_cast<uint32_t const*>(layer3 + 8);
      auto resp_h = reinterpret_cast<uint32_t const*>(layer3 + 24);
      conn.src = {orig_h, address::ipv4, address::network};
      conn.dst = {resp_h, address::ipv4, address::network};
      layer4_proto = *(layer3 + 6);
      layer4 = layer3 + 40;
      payload_size -= 40;
    } break;
  }
  if (layer4_proto == IPPROTO_TCP) {
    VAST_ASSERT(layer4);
    auto orig_p = *reinterpret_cast<uint16_t const*>(layer4);
    auto resp_p = *reinterpret_cast<uint16_t const*>(layer4 + 2);
    orig_p = util::byte_swap<network_endian, host_endian>(orig_p);
    resp_p = util::byte_swap<network_endian, host_endian>(resp_p);
    conn.sport = {orig_p, port::tcp};
    conn.dport = {resp_p, port::tcp};
    auto data_offset = *reinterpret_cast<uint8_t const*>(layer4 + 12) >> 4;
    payload_size -= data_offset * 4;
  } else if (layer4_proto == IPPROTO_UDP) {
    VAST_ASSERT(layer4);
    auto orig_p = *reinterpret_cast<uint16_t const*>(layer4);
    auto resp_p = *reinterpret_cast<uint16_t const*>(layer4 + 2);
    orig_p = util::byte_swap<network_endian, host_endian>(orig_p);
    resp_p = util::byte_swap<network_endian, host_endian>(resp_p);
    conn.sport = {orig_p, port::udp};
    conn.dport = {resp_p, port::udp};
    payload_size -= 8;
  } else if (layer4_proto == IPPROTO_ICMP) {
    VAST_ASSERT(layer4);
    auto message_type = *reinterpret_cast<uint8_t const*>(layer4);
    auto message_code = *reinterpret_cast<uint8_t const*>(layer4 + 1);
    conn.sport = {message_type, port::icmp};
    conn.dport = {message_code, port::icmp};
    payload_size -= 8; // TODO: account for variable-size data.
  }
  // Parse packet timestamp
  uint64_t packet_time = packet_header_->ts.tv_sec;
  if (last_expire_ == 0)
    last_expire_ = packet_time;
  auto i = flows_.find(conn);
  if (i == flows_.end())
    i = flows_.insert(std::make_pair(conn, connection_state{0, packet_time}))
          .first;
  else
    i->second.last = packet_time;
  auto& flow_size = i->second.bytes;
  if (flow_size == cutoff_)
    return {};
  if (flow_size + payload_size <= cutoff_) {
    flow_size += payload_size;
  } else {
    // Trim the last packet so that it fits.
    packet_size -= flow_size + payload_size - cutoff_;
    flow_size = cutoff_;
  }
  // Evict all elements that have been inactive for a while.
  if (packet_time - last_expire_ > expire_interval_) {
    last_expire_ = packet_time;
    auto i = flows_.begin();
    while (i != flows_.end())
      if (packet_time - i->second.last > max_age_)
        i = flows_.erase(i);
      else
        ++i;
  }
  // If the flow table gets too large, we evict a random element.
  if (!flows_.empty() && flows_.size() % max_flows_ == 0) {
    auto buckets = flows_.bucket_count();
    auto unif1 = std::uniform_int_distribution<size_t>{0, buckets - 1};
    auto bucket = unif1(generator_);
    auto bucket_size = flows_.bucket_size(bucket);
    while (bucket_size == 0) {
      ++bucket;
      bucket %= buckets;
      bucket_size = flows_.bucket_size(bucket);
    }
    auto unif2 = std::uniform_int_distribution<size_t>{0, bucket_size - 1};
    auto offset = unif2(generator_);
    VAST_ASSERT(offset < bucket_size);
    auto begin = flows_.begin(bucket);
    for (size_t n = 0; n < offset; ++n)
      ++begin;
    flows_.erase(begin->first);
  }
  // Assemble packet.
  record packet;
  record meta;
  meta.emplace_back(std::move(conn.src));
  meta.emplace_back(std::move(conn.dst));
  meta.emplace_back(std::move(conn.sport));
  meta.emplace_back(std::move(conn.dport));
  packet.emplace_back(std::move(meta));
  // We start with the network layer and skip the link layer.
  auto str = reinterpret_cast<char const*>(data + 14);
  packet.emplace_back(std::string{str, packet_size});
  auto s = std::chrono::seconds(packet_header_->ts.tv_sec);
#ifdef PCAP_TSTAMP_PRECISION_NANO
  auto sub = std::chrono::nanoseconds(packet_header_->ts.tv_usec);
#else
  auto sub = std::chrono::microseconds(packet_header_->ts.tv_usec);
#endif
  auto timestamp = s + sub;
  if (pseudo_realtime_ > 0) {
    if (timestamp < last_timestamp_) {
      VAST_WARN_AT(self, "encountered non-monotonic packet timestamps:",
                   timestamp.count(), '<', last_timestamp_.count());
    }
    if (last_timestamp_ != std::chrono::nanoseconds{}) {
      auto delta = timestamp - last_timestamp_;
      std::this_thread::sleep_for(delta / pseudo_realtime_);
    }
    last_timestamp_ = timestamp;
  }
  event e{{std::move(packet), packet_type_}};
  e.timestamp(time::point{timestamp});
  return std::move(e);
}

behavior pcap(stateful_actor<pcap_state>* self, std::string input,
              uint64_t cutoff, size_t max_flows, size_t max_age,
              size_t expire_interval, int64_t pseudo_realtime) {
  self->state.input_ = std::move(input);
  self->state.cutoff_ = cutoff;
  self->state.max_flows_ = max_flows;
  self->state.max_age_ = max_age;
  self->state.expire_interval_ = expire_interval;
  self->state.pseudo_realtime_ = pseudo_realtime;
  return make(self);
}

} // namespace source
} // namespace vast
