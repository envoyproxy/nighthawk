#pragma once

#include <memory>

#include "envoy/common/pure.h"

namespace Nighthawk {

class TerminationPredicate;

using TerminationPredicatePtr = std::unique_ptr<TerminationPredicate>;

/**
 * Determines if and how execution is terminated.
 */
class TerminationPredicate {
public:
  enum class Status {
    PROCEED,   // indicates execution should proceed
    TERMINATE, // indicates execution should terminate successfully
    FAIL       // indicates execution should terminate unsuccessfully
  };

  virtual ~TerminationPredicate() = default;

  /**
   * Links a child predicated. Will be evaluated first when evaluateChain() is called. Only allowed
   * to be called a single time per instance.
   *
   * @param child the child predicate to link. nullptr is not allowed.
   * @return the dereferenced input child predicate. For convenience, so calls can be chained.
   */
  virtual TerminationPredicate& link(TerminationPredicatePtr&& child) PURE;

  /**
   * Appends a predicate to the last element of the chain.
   *
   * @param child the child predicate to link. nullptr is not allowed.
   * @return the dereferenced input child predicate. For convenience, so calls can be chained.
   */
  virtual TerminationPredicate& appendToChain(TerminationPredicatePtr&& child) PURE;

  /**
   * Recursively evaluates chain of linked predicates, this instance last.
   * If any linked element returns anything other then PROCEED that status will
   * be returned.
   *
   * @return Status as determined by the chain of predicates.
   */
  virtual Status evaluateChain() PURE;

  /**
   * @return Status as determined by this instance.
   */
  virtual Status evaluate() PURE;
};

} // namespace Nighthawk