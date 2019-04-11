#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "common/common/non_copyable.h"
#include "envoy/common/exception.h"
#include "envoy/common/pure.h"

#include "nighthawk/source/client/output.pb.h"

namespace Nighthawk {

class Statistic;

typedef std::unique_ptr<Statistic> StatisticPtr;
typedef std::map<std::string, Statistic const*> StatisticPtrMap;

/**
 * Abstract interface for a statistic.
 */
class Statistic : Envoy::NonCopyable {
public:
  virtual ~Statistic() = default;
  /**
   * Method for adding a sample value.
   * @param value the value of the sample to add
   */
  virtual void addValue(int64_t sample_value) PURE;

  virtual uint64_t count() const PURE;
  virtual double mean() const PURE;
  virtual double pvariance() const PURE;
  virtual double pstdev() const PURE;

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
   * @return std::string a representation of the statistic as a std::string.
   */
  virtual std::string toString() const PURE;

  /**
   * @return nighthawk::client::Statistic a representation of the statistic as a protobuf message.
   */
  virtual nighthawk::client::Statistic toProto() PURE;

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
  virtual void setId(const std::string& id) PURE;
};

} // namespace Nighthawk