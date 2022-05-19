#include "source/server/http_filter_config_base.h"

#include "source/server/well_known_headers.h"

namespace Nighthawk {
namespace Server {

FilterConfigurationBase::FilterConfigurationBase(absl::string_view filter_name)
    : filter_name_(filter_name) {}

bool FilterConfigurationBase::validateOrSendError(
    const absl::Status& effective_config,
    Envoy::Http::StreamDecoderFilterCallbacks& decoder_callbacks) const {
  if (!effective_config.ok()) {
    decoder_callbacks.sendLocalReply(static_cast<Envoy::Http::Code>(500),
                                     fmt::format("{} didn't understand the request: {}",
                                                 filter_name_, effective_config.message()),
                                     nullptr, absl::nullopt, "");
    return true;
  }
  return false;
}

} // namespace Server
} // namespace Nighthawk
