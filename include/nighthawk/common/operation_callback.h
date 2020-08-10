#include <functional>

namespace nighthawk {

/**
 * Callback specification. Done will be true if the flow was fully executed,
 * and false when it was not (e.g. no connection could be made).
 * Success indicates whether the final status of a flow should be considered
 * a success, and is meaningful only when done equals true.
 */
using OperationCallback = std::function<void(bool done, bool success)>;
} // namespace nighthawk