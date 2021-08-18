#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"

namespace Nighthawk {

/**
 * Creates Envoy bootstrap configuration.
 *
 * The created bootstrap configuration can be used to upstream requests to the
 * specified uris.
 *
 * @param dispatcher is used when resolving hostnames to IP addresses in the
          bootstrap.
 * @param options are the options this Nighthawk execution was triggered with.
 * @param number_of_workers indicates how many Nighthawk workers will be
 *        upstreaming requests. A separate cluster is generated for each worker.
 *
 * @return the created bootstrap configuration.
 */
absl::StatusOr<envoy::config::bootstrap::v3::Bootstrap>
createBootstrapConfiguration(Envoy::Event::Dispatcher& dispatcher, const Client::Options& options,
                             int number_of_workers);

} // namespace Nighthawk
