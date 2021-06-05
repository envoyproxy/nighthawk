#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "envoy/common/exception.h"
#include "envoy/common/pure.h"

#include "external/envoy/source/common/common/non_copyable.h"

#include "api/client/output.pb.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

class Statistic;

using StatisticPtr = std::unique_ptr<Statistic>;
using StatisticPtrMap = std::map<std::string, Statistic const*>;

/**
 * Abstract interface for a statistic.
 */
class Statistic : Envoy::NonCopyable {
public:
  enum class SerializationDomain { RAW, DURATION };

  virtual ~Statistic() = default;

  /**
   * Method for adding a sample value.
   * @param value the value of the sample to add
   */
  virtual void addValue(uint64_t sample_value) PURE;

  /**
   * @return uint64_t The number of sampled values.
   */
  virtual uint64_t count() const PURE;

  /**
   * @return double Mean derived from the sampled values.
   */
  virtual double mean() const PURE;

  /**
   * @return double Variance derived from the sampled values.
   */
  virtual double pvariance() const PURE;

  /**
   * @return double Standard deviation derived from the sampled values.
   */
  virtual double pstdev() const PURE;

  /**
   * @return uint64_t The smallest sampled value.
   */
  virtual uint64_t min() const PURE;

  /**
   * @return uint64_t The largest sampled value.
   */
  virtual uint64_t max() const PURE;

  /**
   * @return StatisticPtr Yields a new instance of the same type as the instance this is called on.
   */
  virtual StatisticPtr createNewInstanceOfSameType() const PURE;

  /**
   * Only used in tests to match expectations to the right precision level.
   * @return uint64_t the number of significant digits. 0 is assumed to be max precision.
   */
  virtual uint64_t significantDigits() const { return 0; }

  /**
   * Indicates if the implementation is subject to catastrophic cancellation.
   * Used in tests.
   * @return True iff catastrophic cancellation should not occur.
   */
  virtual bool resistsCatastrophicCancellation() const { return false; }

  /**
   * @return std::string Gets a string representation of the statistic as a std::string.
   */
  virtual std::string toString() const PURE;

  /**
   * @param domain Used to indicate if serialization should represent durations or raw values.
   * @return nighthawk::client::Statistic a representation of the statistic as a protobuf message.
   */
  virtual nighthawk::client::Statistic toProto(SerializationDomain domain) const PURE;

  /**
   * Combines two Statistics into one, and returns a new, merged, Statistic.
   * This is useful for computing results from multiple workers into a
   * single global view. Types of the Statistics objects that will be combined
   * must be the same, or else a std::bad_cast exception will be raised.
   * @param statistic The Statistic that should be combined with this instance.
   * @return StatisticPtr instance.
   */
  virtual StatisticPtr combine(const Statistic& statistic) const PURE;

  /**
   * Gets the id of the Statistic instance, which is an empty string when not set.
   * @return std::string The id of the Statistic instance.
   */
  virtual std::string id() const PURE;

  /**
   * Sets the id of the Statistic instance.
   * @param id The id that should be set for the Statistic instance.
   */
  virtual void setId(absl::string_view id) PURE;

  /**
   * Build a string representation of this Statistic instance.
   *
   * @return absl::StatusOr<std::unique_ptr<std::istream>> Status or a stream that will yield
   * a serialized representation of this Statistic instance.
   */
  virtual absl::StatusOr<std::unique_ptr<std::istream>> serializeNative() const PURE;

  /**
   *  Reconstruct this Statistic instance using the serialization delivered by the input stream.
   *
   * @param input_stream Stream that will deliver a serialized representation.
   * @return absl::Status Status indicating success or failure. Upon success the statistic
   * instance this was called for will now represent what the stream contained.
   */
  virtual absl::Status deserializeNative(std::istream& input_stream) PURE;
};

} // namespace Nighthawk