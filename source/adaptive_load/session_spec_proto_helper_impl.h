#include "nighthawk/adaptive_load/session_spec_proto_helper.h"

namespace Nighthawk {

class AdaptiveLoadSessionSpecProtoHelperImpl : public AdaptiveLoadSessionSpecProtoHelper {
public:
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec
  SetDefaults(nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec) override;

  absl::Status
  CheckSessionSpec(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) override;
};

} // namespace Nighthawk
