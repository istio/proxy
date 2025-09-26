#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "internal/align.h"
#include "internal/new.h"

namespace google::api::expr::runtime {

// CelValue stack.
// Implementation is based on vector to allow passing parameters from
// stack as Span<>.
class EvaluatorStack {
 public:
  explicit EvaluatorStack(size_t max_size) { Reserve(max_size); }

  EvaluatorStack(const EvaluatorStack&) = delete;
  EvaluatorStack(EvaluatorStack&&) = delete;

  ~EvaluatorStack() {
    if (max_size() > 0) {
      const size_t n = size();
      std::destroy_n(values_begin_, n);
      std::destroy_n(attributes_begin_, n);
      cel::internal::SizedDelete(data_, SizeBytes(max_size_));
    }
  }

  EvaluatorStack& operator=(const EvaluatorStack&) = delete;
  EvaluatorStack& operator=(EvaluatorStack&&) = delete;

  // Return the current stack size.
  size_t size() const {
    ABSL_DCHECK_GE(values_, values_begin_);
    ABSL_DCHECK_LE(values_, values_begin_ + max_size_);
    ABSL_DCHECK_GE(attributes_, attributes_begin_);
    ABSL_DCHECK_LE(attributes_, attributes_begin_ + max_size_);
    ABSL_DCHECK_EQ(values_ - values_begin_, attributes_ - attributes_begin_);

    return values_ - values_begin_;
  }

  // Return the maximum size of the stack.
  size_t max_size() const {
    ABSL_DCHECK_GE(values_, values_begin_);
    ABSL_DCHECK_LE(values_, values_begin_ + max_size_);
    ABSL_DCHECK_GE(attributes_, attributes_begin_);
    ABSL_DCHECK_LE(attributes_, attributes_begin_ + max_size_);
    ABSL_DCHECK_EQ(values_ - values_begin_, attributes_ - attributes_begin_);

    return max_size_;
  }

  // Returns true if stack is empty.
  bool empty() const {
    ABSL_DCHECK_GE(values_, values_begin_);
    ABSL_DCHECK_LE(values_, values_begin_ + max_size_);
    ABSL_DCHECK_GE(attributes_, attributes_begin_);
    ABSL_DCHECK_LE(attributes_, attributes_begin_ + max_size_);
    ABSL_DCHECK_EQ(values_ - values_begin_, attributes_ - attributes_begin_);

    return values_ == values_begin_;
  }

  bool full() const {
    ABSL_DCHECK_GE(values_, values_begin_);
    ABSL_DCHECK_LE(values_, values_begin_ + max_size_);
    ABSL_DCHECK_GE(attributes_, attributes_begin_);
    ABSL_DCHECK_LE(attributes_, attributes_begin_ + max_size_);
    ABSL_DCHECK_EQ(values_ - values_begin_, attributes_ - attributes_begin_);

    return values_ == values_end_;
  }

  // Attributes stack size.
  ABSL_DEPRECATED("Use size()")
  size_t attribute_size() const { return size(); }

  // Check that stack has enough elements.
  bool HasEnough(size_t size) const { return this->size() >= size; }

  // Dumps the entire stack state as is.
  void Clear() {
    if (max_size() > 0) {
      const size_t n = size();
      std::destroy_n(values_begin_, n);
      std::destroy_n(attributes_begin_, n);

      ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(
          values_begin_, values_begin_ + max_size_, values_, values_begin_);
      ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(attributes_begin_,
                                         attributes_begin_ + max_size_,
                                         attributes_, attributes_begin_);

      values_ = values_begin_;
      attributes_ = attributes_begin_;
    }
  }

  // Gets the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const cel::Value> GetSpan(size_t size) const {
    ABSL_DCHECK(HasEnough(size));

    return absl::Span<const cel::Value>(values_ - size, size);
  }

  // Gets the last size attribute trails of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const AttributeTrail> GetAttributeSpan(size_t size) const {
    ABSL_DCHECK(HasEnough(size));

    return absl::Span<const AttributeTrail>(attributes_ - size, size);
  }

  // Peeks the last element of the stack.
  // Checking that stack is not empty is caller's responsibility.
  cel::Value& Peek() {
    ABSL_DCHECK(HasEnough(1));

    return *(values_ - 1);
  }

  // Peeks the last element of the stack.
  // Checking that stack is not empty is caller's responsibility.
  const cel::Value& Peek() const {
    ABSL_DCHECK(HasEnough(1));

    return *(values_ - 1);
  }

  // Peeks the last element of the attribute stack.
  // Checking that stack is not empty is caller's responsibility.
  const AttributeTrail& PeekAttribute() const {
    ABSL_DCHECK(HasEnough(1));

    return *(attributes_ - 1);
  }

  // Peeks the last element of the attribute stack.
  // Checking that stack is not empty is caller's responsibility.
  AttributeTrail& PeekAttribute() {
    ABSL_DCHECK(HasEnough(1));

    return *(attributes_ - 1);
  }

  void Pop() {
    ABSL_DCHECK(!empty());

    --values_;
    values_->~Value();
    --attributes_;
    attributes_->~AttributeTrail();

    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(values_begin_, values_begin_ + max_size_,
                                       values_ + 1, values_);
    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(attributes_begin_,
                                       attributes_begin_ + max_size_,
                                       attributes_ + 1, attributes_);
  }

  // Clears the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  void Pop(size_t size) {
    ABSL_DCHECK(HasEnough(size));

    for (; size > 0; --size) {
      Pop();
    }
  }

  template <typename V, typename A,
            typename = std::enable_if_t<
                std::conjunction_v<std::is_convertible<V, cel::Value>,
                                   std::is_convertible<A, AttributeTrail>>>>
  void Push(V&& value, A&& attribute) {
    ABSL_DCHECK(!full());

    if (ABSL_PREDICT_FALSE(full())) {
      Grow();
    }

    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(values_begin_, values_begin_ + max_size_,
                                       values_, values_ + 1);
    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(attributes_begin_,
                                       attributes_begin_ + max_size_,
                                       attributes_, attributes_ + 1);

    ::new (static_cast<void*>(values_++)) cel::Value(std::forward<V>(value));
    ::new (static_cast<void*>(attributes_++))
        AttributeTrail(std::forward<A>(attribute));
  }

  template <typename V,
            typename = std::enable_if_t<std::is_convertible_v<V, cel::Value>>>
  void Push(V&& value) {
    ABSL_DCHECK(!full());

    Push(std::forward<V>(value), absl::nullopt);
  }

  // Equivalent to `PopAndPush(1, ...)`.
  template <typename V, typename A,
            typename = std::enable_if_t<
                std::conjunction_v<std::is_convertible<V, cel::Value>,
                                   std::is_convertible<A, AttributeTrail>>>>
  void PopAndPush(V&& value, A&& attribute) {
    ABSL_DCHECK(!empty());

    *(values_ - 1) = std::forward<V>(value);
    *(attributes_ - 1) = std::forward<A>(attribute);
  }

  // Equivalent to `PopAndPush(1, ...)`.
  template <typename V,
            typename = std::enable_if_t<std::is_convertible_v<V, cel::Value>>>
  void PopAndPush(V&& value) {
    ABSL_DCHECK(!empty());

    PopAndPush(std::forward<V>(value), absl::nullopt);
  }

  // Equivalent to `Pop(n)` followed by `Push(...)`. Both `V` and `A` MUST NOT
  // be located on the stack. If this is the case, use SwapAndPop instead.
  template <typename V, typename A,
            typename = std::enable_if_t<
                std::conjunction_v<std::is_convertible<V, cel::Value>,
                                   std::is_convertible<A, AttributeTrail>>>>
  void PopAndPush(size_t n, V&& value, A&& attribute) {
    if (n > 0) {
      if constexpr (std::is_same_v<cel::Value, absl::remove_cvref_t<V>>) {
        ABSL_DCHECK(&value < values_begin_ ||
                    &value >= values_begin_ + max_size_)
            << "Attmpting to push a value about to be popped, use PopAndSwap "
               "instead.";
      }
      if constexpr (std::is_same_v<AttributeTrail, absl::remove_cvref_t<A>>) {
        ABSL_DCHECK(&attribute < attributes_begin_ ||
                    &attribute >= attributes_begin_ + max_size_)
            << "Attmpting to push an attribute about to be popped, use "
               "PopAndSwap instead.";
      }

      Pop(n - 1);

      ABSL_DCHECK(!empty());

      *(values_ - 1) = std::forward<V>(value);
      *(attributes_ - 1) = std::forward<A>(attribute);
    } else {
      Push(std::forward<V>(value), std::forward<A>(attribute));
    }
  }

  // Equivalent to `Pop(n)` followed by `Push(...)`. `V` MUST NOT be located on
  // the stack. If this is the case, use SwapAndPop instead.
  template <typename V,
            typename = std::enable_if_t<std::is_convertible_v<V, cel::Value>>>
  void PopAndPush(size_t n, V&& value) {
    PopAndPush(n, std::forward<V>(value), absl::nullopt);
  }

  // Swaps the `n - i` element (from the top of the stack) with the `n` element,
  // and pops `n - 1` elements. This results in the `n - i` element being at the
  // top of the stack.
  void SwapAndPop(size_t n, size_t i) {
    ABSL_DCHECK_GT(n, 0);
    ABSL_DCHECK_LT(i, n);
    ABSL_DCHECK(HasEnough(n - 1));

    using std::swap;

    if (i > 0) {
      swap(*(values_ - n), *(values_ - n + i));
      swap(*(attributes_ - n), *(attributes_ - n + i));
    }
    Pop(n - 1);
  }

  // Update the max size of the stack and update capacity if needed.
  void SetMaxSize(size_t size) { Reserve(size); }

 private:
  static size_t AttributesBytesOffset(size_t size) {
    return cel::internal::AlignUp(sizeof(cel::Value) * size,
                                  __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  }

  static size_t SizeBytes(size_t size) {
    return AttributesBytesOffset(size) + (sizeof(AttributeTrail) * size);
  }

  void Grow();

  // Preallocate stack.
  void Reserve(size_t size);

  cel::Value* absl_nullability_unknown values_ = nullptr;
  cel::Value* absl_nullability_unknown values_begin_ = nullptr;
  AttributeTrail* absl_nullability_unknown attributes_ = nullptr;
  AttributeTrail* absl_nullability_unknown attributes_begin_ = nullptr;
  cel::Value* absl_nullability_unknown values_end_ = nullptr;
  void* absl_nullability_unknown data_ = nullptr;
  size_t max_size_ = 0;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_
