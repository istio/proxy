#include "eval/eval/evaluator_stack.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/dynamic_annotations.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_log.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "internal/new.h"

namespace google::api::expr::runtime {

void EvaluatorStack::Grow() {
  const size_t new_max_size = std::max(max_size() * 2, size_t{1});
  ABSL_LOG(ERROR) << "evaluation stack is unexpectedly full: growing from "
                  << max_size() << " to " << new_max_size
                  << " as a last resort to avoid crashing: this should not "
                     "have happened so there must be a bug somewhere in "
                     "the planner or evaluator";
  Reserve(new_max_size);
}

void EvaluatorStack::Reserve(size_t size) {
  static_assert(alignof(cel::Value) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  static_assert(alignof(AttributeTrail) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);

  if (max_size_ >= size) {
    return;
  }

  void* absl_nullability_unknown data = cel::internal::New(SizeBytes(size));

  cel::Value* absl_nullability_unknown values_begin =
      reinterpret_cast<cel::Value*>(data);
  cel::Value* absl_nullability_unknown values = values_begin;

  AttributeTrail* absl_nullability_unknown attributes_begin =
      reinterpret_cast<AttributeTrail*>(reinterpret_cast<uint8_t*>(data) +
                                        AttributesBytesOffset(size));
  AttributeTrail* absl_nullability_unknown attributes = attributes_begin;

  if (max_size_ > 0) {
    const size_t n = this->size();
    const size_t m = std::min(n, size);

    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(values_begin, values_begin + size,
                                       values_begin + size, values + m);
    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(attributes_begin,
                                       attributes_begin + size,
                                       attributes_begin + size, attributes + m);

    for (size_t i = 0; i < m; ++i) {
      ::new (static_cast<void*>(values++))
          cel::Value(std::move(values_begin_[i]));
      ::new (static_cast<void*>(attributes++))
          AttributeTrail(std::move(attributes_begin_[i]));
    }
    std::destroy_n(values_begin_, n);
    std::destroy_n(attributes_begin_, n);

    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(values_begin_, values_begin_ + max_size_,
                                       values_, values_begin_ + max_size_);
    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(
        attributes_begin_, attributes_begin_ + max_size_, attributes_,
        attributes_begin_ + max_size_);

    cel::internal::SizedDelete(data_, SizeBytes(max_size_));
  } else {
    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(values_begin, values_begin + size,
                                       values_begin + size, values);
    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(attributes_begin,
                                       attributes_begin + size,
                                       attributes_begin + size, attributes);
  }

  values_ = values;
  values_begin_ = values_begin;
  values_end_ = values_begin + size;

  attributes_ = attributes;
  attributes_begin_ = attributes_begin;

  data_ = data;
  max_size_ = size;
}

}  // namespace google::api::expr::runtime
