#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"

namespace Nighthawk {

/**
 * Creates Envoy bootstrap configuration.
 *
 * The created bootstrap configuration can be used to upstream requests to the
 * specified uris.
 *
 * @param options are the options this Nighthawk execution was triggered with.
 * @param uris are the endpoints to which the requests will be upstreamed. At
 *        least one uri must be specified. It is assumed that all the uris have
 *        the same scheme (e.g. https). All the uri objects must already be
 *        resolved.
 * @param request_source_uri is the address of the request source service to
 *        use, can be NULL if request source isn't used. If not NULL, the uri
 *        object must already be resolved.
 * @param number_of_workers indicates how many Nighthawk workers will be
 *        upstreaming requests. A separate cluster is generated for each worker.
 *
 * @return the created bootstrap configuration.
 */
absl::StatusOr<envoy::config::bootstrap::v3::Bootstrap>
createBootstrapConfiguration(const Client::Options& options, const std::vector<UriPtr>& uris,
                             const UriPtr& request_source_uri, int number_of_workers);

} // namespace Nighthawk
