#include "source/common/statsd_sink.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <cstdio>

#include "envoy/network/socket_interface.h"
#include "envoy/registry/registry.h"

#include "external/envoy/source/common/buffer/buffer_impl.h"
#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/network/address_impl.h"
#include "external/envoy/source/common/network/resolver_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "source/common/statistic_impl.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace Nighthawk {

namespace {
constexpr absl::string_view kDefaultPrefix = "nighthawk";

std::string formatDouble(double value) {
  char buffer[64];
  const int n = std::snprintf(buffer, sizeof(buffer), "%.3f", value);
  RELEASE_ASSERT(n > 0 && n < static_cast<int>(sizeof(buffer)), "formatting failed");
  return std::string(buffer, n);
}
} // namespace

StatsdSink::StatsdSink(Envoy::Network::Address::InstanceConstSharedPtr address,
                       absl::string_view prefix, bool use_tags,
                       std::optional<uint64_t> max_bytes_per_datagram,
                       const std::vector<std::string>& extra_tags)
    : address_(std::move(address)), io_handle_(Envoy::Network::ioHandleForAddr(
                                        Envoy::Network::Socket::Type::Datagram, address_, {})),
      prefix_(prefix.empty() ? std::string(kDefaultPrefix) : std::string(prefix)),
      use_tags_(use_tags), max_bytes_per_datagram_(max_bytes_per_datagram),
      extra_tags_(use_tags && !extra_tags.empty()
                      ? absl::StrCat(",", absl::StrJoin(extra_tags, ","))
                      : "") {
  RELEASE_ASSERT(address_ != nullptr, "statsd sink needs an address");
  if (!use_tags && !extra_tags.empty()) {
    ENVOY_LOG(warn, "stats sink tags are ignored by the plain statsd sink; use dog_statsd");
  }
}

StatsdSink::~StatsdSink() { flushAllSamples(); }

std::pair<std::string, std::optional<int>> StatsdSink::splitWorkerPrefix(absl::string_view name) {
  // Nighthawk scopes per-worker stats as "cluster.<n>.<rest>" and "worker.<n>.<rest>".
  std::vector<absl::string_view> parts = absl::StrSplit(name, absl::MaxSplits('.', 2));
  if (parts.size() == 3 && (parts[0] == "cluster" || parts[0] == "worker")) {
    int worker_id;
    if (absl::SimpleAtoi(parts[1], &worker_id)) {
      return {std::string(parts[2]), worker_id};
    }
  }
  return {std::string(name), std::nullopt};
}

std::string StatsdSink::buildMessage(absl::string_view store_name, absl::string_view value,
                                     absl::string_view type, std::optional<int> worker_id) const {
  std::string name(store_name);
  if (!worker_id.has_value()) {
    std::tie(name, worker_id) = splitWorkerPrefix(store_name);
  }
  if (!use_tags_ && worker_id.has_value()) {
    name = absl::StrCat("worker.", worker_id.value(), ".", name);
  }
  std::string message = absl::StrCat(prefix_, ".", name, ":", value, "|", type);
  if (use_tags_) {
    if (worker_id.has_value()) {
      absl::StrAppend(&message, "|#worker:", worker_id.value(), extra_tags_);
    } else if (!extra_tags_.empty()) {
      absl::StrAppend(&message, "|#", absl::string_view(extra_tags_).substr(1));
    }
  }
  return message;
}

std::string StatsdSink::formatNanosAsMillis(uint64_t nanos) {
  return formatDouble(static_cast<double>(nanos) / 1e6);
}

void StatsdSink::send(absl::string_view message) {
  Envoy::Buffer::RawSlice slice{const_cast<char*>(message.data()), message.size()};
  absl::MutexLock lock(&socket_mutex_);
  Envoy::Network::Utility::writeToSocket(*io_handle_, &slice, 1, nullptr, *address_);
}

void StatsdSink::enqueueSample(std::string message) {
  if (!max_bytes_per_datagram_.has_value()) {
    send(message);
    return;
  }
  std::shared_ptr<SampleBatch> batch;
  {
    absl::MutexLock lock(&batches_mutex_);
    std::shared_ptr<SampleBatch>& slot = batches_[std::this_thread::get_id()];
    if (slot == nullptr) {
      slot = std::make_shared<SampleBatch>();
    }
    batch = slot;
  }
  // Only this thread appends to its batch; flushAllSamples() runs after the threads stopped.
  if (!batch->data.empty() &&
      batch->data.size() + 1 + message.size() > max_bytes_per_datagram_.value()) {
    send(batch->data);
    batch->data.clear();
  }
  if (!batch->data.empty()) {
    batch->data.push_back('\n');
  }
  batch->data.append(message);
}

void StatsdSink::flushSamples() {
  std::shared_ptr<SampleBatch> batch;
  {
    absl::MutexLock lock(&batches_mutex_);
    auto it = batches_.find(std::this_thread::get_id());
    if (it == batches_.end()) {
      return;
    }
    batch = it->second;
  }
  if (!batch->data.empty()) {
    send(batch->data);
    batch->data.clear();
  }
}

void StatsdSink::flushAllSamples() {
  std::vector<std::shared_ptr<SampleBatch>> batches;
  {
    absl::MutexLock lock(&batches_mutex_);
    for (auto& entry : batches_) {
      batches.push_back(entry.second);
    }
  }
  for (const std::shared_ptr<SampleBatch>& batch : batches) {
    if (!batch->data.empty()) {
      send(batch->data);
      batch->data.clear();
    }
  }
}

void StatsdSink::sendBatch(const std::vector<std::string>& messages) {
  if (!max_bytes_per_datagram_.has_value()) {
    for (const std::string& message : messages) {
      send(message);
    }
    return;
  }
  const uint64_t max_bytes = max_bytes_per_datagram_.value();
  std::string datagram;
  for (const std::string& message : messages) {
    if (!datagram.empty() && datagram.size() + 1 + message.size() > max_bytes) {
      send(datagram);
      datagram.clear();
    }
    if (!datagram.empty()) {
      datagram.push_back('\n');
    }
    datagram.append(message);
  }
  if (!datagram.empty()) {
    send(datagram);
  }
}

void StatsdSink::flush(Envoy::Stats::MetricSnapshot& snapshot) {
  flushSamples();
  std::vector<std::string> messages;
  for (const auto& counter : snapshot.counters()) {
    if (counter.delta_ > 0) {
      messages.push_back(buildMessage(counter.counter_.get().name(), absl::StrCat(counter.delta_),
                                      "c", std::nullopt));
    }
  }
  for (const auto& counter : snapshot.hostCounters()) {
    if (counter.delta() > 0) {
      messages.push_back(
          buildMessage(counter.name(), absl::StrCat(counter.delta()), "c", std::nullopt));
    }
  }
  for (const auto& gauge : snapshot.gauges()) {
    messages.push_back(
        buildMessage(gauge.get().name(), absl::StrCat(gauge.get().value()), "g", std::nullopt));
  }
  for (const auto& gauge : snapshot.hostGauges()) {
    messages.push_back(buildMessage(gauge.name(), absl::StrCat(gauge.value()), "g", std::nullopt));
  }
  sendBatch(messages);
}

void StatsdSink::onHistogramComplete(const Envoy::Stats::Histogram& histogram, uint64_t value) {
  // Nighthawk's own statistics record nanoseconds and report Unit::Unspecified; Envoy histograms
  // carry their unit. statsd only knows millisecond timings ("|ms") and raw values ("|h").
  std::optional<int> worker_id;
  if (const auto* sinkable = dynamic_cast<const SinkableStatistic*>(&histogram);
      sinkable != nullptr) {
    worker_id = sinkable->worker_id();
  }
  std::string message;
  switch (histogram.unit()) {
  case Envoy::Stats::Histogram::Unit::Unspecified:
    message = buildMessage(histogram.name(), formatNanosAsMillis(value), "ms", worker_id);
    break;
  case Envoy::Stats::Histogram::Unit::Microseconds:
    message = buildMessage(histogram.name(), formatDouble(static_cast<double>(value) / 1e3), "ms",
                           worker_id);
    break;
  case Envoy::Stats::Histogram::Unit::Milliseconds:
    message = buildMessage(histogram.name(), absl::StrCat(value), "ms", worker_id);
    break;
  case Envoy::Stats::Histogram::Unit::Percent:
    message = buildMessage(
        histogram.name(),
        formatDouble(static_cast<double>(value) / Envoy::Stats::Histogram::PercentScale), "h",
        worker_id);
    break;
  case Envoy::Stats::Histogram::Unit::Bytes:
  default:
    message = buildMessage(histogram.name(), absl::StrCat(value), "h", worker_id);
    break;
  }
  enqueueSample(std::move(message));
}

namespace {
// Resolves a host name synchronously (sinks are created once, at startup).
Envoy::Network::Address::InstanceConstSharedPtr resolveHostName(const std::string& host,
                                                                uint32_t port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* results = nullptr;
  const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &results);
  if (rc != 0 || results == nullptr) {
    throw Envoy::EnvoyException(
        absl::StrCat("statsd sink: could not resolve host '", host, "': ", gai_strerror(rc)));
  }
  Envoy::Network::Address::InstanceConstSharedPtr address;
  for (addrinfo* entry = results; entry != nullptr && address == nullptr; entry = entry->ai_next) {
    sockaddr_storage storage{};
    if (entry->ai_addrlen > sizeof(storage)) {
      continue;
    }
    std::copy_n(reinterpret_cast<const uint8_t*>(entry->ai_addr), entry->ai_addrlen,
                reinterpret_cast<uint8_t*>(&storage));
    if (storage.ss_family == AF_INET) {
      reinterpret_cast<sockaddr_in*>(&storage)->sin_port = htons(port);
    } else if (storage.ss_family == AF_INET6) {
      reinterpret_cast<sockaddr_in6*>(&storage)->sin6_port = htons(port);
    } else {
      continue;
    }
    absl::StatusOr<Envoy::Network::Address::InstanceConstSharedPtr> parsed =
        Envoy::Network::Address::addressFromSockAddr(storage, entry->ai_addrlen,
                                                     /*v6only=*/false);
    if (parsed.ok()) {
      address = parsed.value();
    }
  }
  ::freeaddrinfo(results);
  if (address == nullptr) {
    throw Envoy::EnvoyException(
        absl::StrCat("statsd sink: host '", host, "' has no usable IP address"));
  }
  return address;
}

Envoy::Network::Address::InstanceConstSharedPtr
resolveUdpAddress(const envoy::config::core::v3::Address& address) {
  absl::StatusOr<Envoy::Network::Address::InstanceConstSharedPtr> resolved =
      Envoy::Network::Address::resolveProtoAddress(address);
  if (!resolved.ok()) {
    // resolveProtoAddress only accepts IP literals; fall back to resolving a host name.
    if (address.has_socket_address() && address.socket_address().resolver_name().empty() &&
        address.socket_address().port_specifier_case() ==
            envoy::config::core::v3::SocketAddress::PortSpecifierCase::kPortValue) {
      return resolveHostName(address.socket_address().address(),
                             address.socket_address().port_value());
    }
    throw Envoy::EnvoyException(
        absl::StrCat("statsd sink: invalid address: ", resolved.status().message()));
  }
  if (resolved.value()->type() != Envoy::Network::Address::Type::Ip) {
    throw Envoy::EnvoyException("statsd sink: an IP address (UDP) is required");
  }
  return resolved.value();
}
} // namespace

std::unique_ptr<Envoy::Stats::Sink> StatsdSinkFactory::createStatsSink(
    const Envoy::Protobuf::Message& config, Envoy::Stats::SymbolTable&,
    Envoy::ThreadLocal::SlotAllocator& /*tls*/, const std::vector<std::string>& tags) {
  const auto& sink_config = dynamic_cast<const envoy::config::metrics::v3::StatsdSink&>(config);
  if (sink_config.statsd_specifier_case() !=
      envoy::config::metrics::v3::StatsdSink::StatsdSpecifierCase::kAddress) {
    throw Envoy::EnvoyException(
        "statsd sink: only the UDP 'address' form is supported (tcp_cluster_name is not)");
  }
  return std::make_unique<StatsdSink>(resolveUdpAddress(sink_config.address()),
                                      sink_config.prefix(), /*use_tags=*/false, std::nullopt, tags);
}

Envoy::ProtobufTypes::MessagePtr StatsdSinkFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::config::metrics::v3::StatsdSink>();
}

std::unique_ptr<Envoy::Stats::Sink> DogStatsdSinkFactory::createStatsSink(
    const Envoy::Protobuf::Message& config, Envoy::Stats::SymbolTable&,
    Envoy::ThreadLocal::SlotAllocator& /*tls*/, const std::vector<std::string>& tags) {
  const auto& sink_config = dynamic_cast<const envoy::config::metrics::v3::DogStatsdSink&>(config);
  if (!sink_config.has_address()) {
    throw Envoy::EnvoyException("dog_statsd sink: 'address' is required");
  }
  std::optional<uint64_t> max_bytes;
  if (sink_config.has_max_bytes_per_datagram()) {
    max_bytes = sink_config.max_bytes_per_datagram().value();
  }
  return std::make_unique<StatsdSink>(resolveUdpAddress(sink_config.address()),
                                      sink_config.prefix(), /*use_tags=*/true, max_bytes, tags);
}

Envoy::ProtobufTypes::MessagePtr DogStatsdSinkFactory::createEmptyConfigProto() {
  return std::make_unique<envoy::config::metrics::v3::DogStatsdSink>();
}

REGISTER_FACTORY(StatsdSinkFactory, NighthawkStatsSinkFactory);
REGISTER_FACTORY(DogStatsdSinkFactory, NighthawkStatsSinkFactory);

} // namespace Nighthawk
