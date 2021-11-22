#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/network/dns_resolver/dns_factory_util.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"

namespace Nighthawk {

/**
 * Creates Envoy bootstrap configuration.
 *
 * The created bootstrap configuration can be used to upstream requests to the
 * specified uris.
 *
 * @param dispatcher is used when resolving hostnames to IP addresses in the
 * bootstrap.
 * @param options are the options this Nighthawk execution was triggered with.
 * @param dns_resolver_factory used to create a DNS resolver to resolve hostnames
 * in the bootstrap.
 * @param typed_dns_resolver_config config used when creating dns_resolver_factory,
 * also needed when creating the resolver.
 * @param number_of_workers indicates how many Nighthawk workers will be
 *        upstreaming requests. A separate cluster is generated for each worker.
 *
 * @return the created bootstrap configuration.
 */
absl::StatusOr<envoy::config::bootstrap::v3::Bootstrap> createBootstrapConfiguration(
    Envoy::Event::Dispatcher& dispatcher, Envoy::Api::Api& api, const Client::Options& options,
    Envoy::Network::DnsResolverFactory& dns_resolver_factory,
    const envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config,
    int number_of_workers);

} // namespace Nighthawk
