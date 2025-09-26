#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_VALUE_PRODUCER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_VALUE_PRODUCER_H_

#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// CelValueProducer produces CelValue during CEL Expression evaluation.
// It is intended to be used with Activation, to provide on-demand CelValue
// calculations.
// ValueProducer serves as performance optimization. Multiple calls to value
// producer during the execution of the same expression should return the same
// value.
class CelValueProducer {
 public:
  virtual ~CelValueProducer() {}

  // Produces CelValue.
  // If CelValue payload is not a primitive type, it must be owned by arena.
  virtual CelValue Produce(google::protobuf::Arena* arena) = 0;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_VALUE_PRODUCER_H_
