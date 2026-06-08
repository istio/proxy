#include "eval/public/set_util.h"

#include <algorithm>
#include <vector>

namespace google::api::expr::runtime {
namespace {

// Default implementation is operator<.
// Note: for UnknownSet, Error and Message, this is ptr less than.
template <typename T>
int ComparisonImpl(T lhs, T rhs) {
  if (lhs < rhs) {
    return -1;
  } else if (lhs > rhs) {
    return 1;
  } else {
    return 0;
  }
}

template <>
int ComparisonImpl(const CelError* lhs, const CelError* rhs) {
  if (*lhs == *rhs) {
    return 0;
  }
  return lhs < rhs ? -1 : 1;
}

// Message wrapper specialization
template <>
int ComparisonImpl(CelValue::MessageWrapper lhs_wrapper,
                   CelValue::MessageWrapper rhs_wrapper) {
  auto* lhs = lhs_wrapper.message_ptr();
  auto* rhs = rhs_wrapper.message_ptr();
  if (lhs < rhs) {
    return -1;
  } else if (lhs > rhs) {
    return 1;
  } else {
    return 0;
  }
}

// List specialization -- compare size then elementwise compare.
template <>
int ComparisonImpl(const CelList* lhs, const CelList* rhs) {
  int size_comparison = ComparisonImpl(lhs->size(), rhs->size());
  if (size_comparison != 0) {
    return size_comparison;
  }
  google::protobuf::Arena arena;
  for (int i = 0; i < lhs->size(); i++) {
    CelValue lhs_i = lhs->Get(&arena, i);
    CelValue rhs_i = rhs->Get(&arena, i);
    int value_comparison = CelValueCompare(lhs_i, rhs_i);
    if (value_comparison != 0) {
      return value_comparison;
    }
  }
  // equal
  return 0;
}

// Map specialization -- size then sorted elementwise compare (i.e.
// <lhs_key_i, lhs_value_i> < <rhs_key_i, rhs_value_i>
//
// This is expensive, but hopefully maps will be rarely used in sets.
template <>
int ComparisonImpl(const CelMap* lhs, const CelMap* rhs) {
  int size_comparison = ComparisonImpl(lhs->size(), rhs->size());
  if (size_comparison != 0) {
    return size_comparison;
  }

  google::protobuf::Arena arena;

  std::vector<CelValue> lhs_keys;
  std::vector<CelValue> rhs_keys;
  lhs_keys.reserve(lhs->size());
  rhs_keys.reserve(lhs->size());

  const CelList* lhs_key_view = lhs->ListKeys(&arena).value();
  const CelList* rhs_key_view = rhs->ListKeys(&arena).value();

  for (int i = 0; i < lhs->size(); i++) {
    lhs_keys.push_back(lhs_key_view->Get(&arena, i));
    rhs_keys.push_back(rhs_key_view->Get(&arena, i));
  }

  std::sort(lhs_keys.begin(), lhs_keys.end(), &CelValueLessThan);
  std::sort(rhs_keys.begin(), rhs_keys.end(), &CelValueLessThan);

  for (size_t i = 0; i < lhs_keys.size(); i++) {
    auto lhs_key_i = lhs_keys[i];
    auto rhs_key_i = rhs_keys[i];
    int key_comparison = CelValueCompare(lhs_key_i, rhs_key_i);
    if (key_comparison != 0) {
      return key_comparison;
    }

    // keys equal, compare values.
    auto lhs_value_i = lhs->Get(&arena, lhs_key_i).value();
    auto rhs_value_i = rhs->Get(&arena, rhs_key_i).value();
    int value_comparison = CelValueCompare(lhs_value_i, rhs_value_i);
    if (value_comparison != 0) {
      return value_comparison;
    }
  }
  // maps equal
  return 0;
}

struct ComparisonVisitor {
  explicit ComparisonVisitor(CelValue rhs) : rhs(rhs) {}
  template <typename T>
  int operator()(T lhs_value) {
    T rhs_value;
    if (!rhs.GetValue(&rhs_value)) {
      return ComparisonImpl(CelValue::Type(CelValue::IndexOf<T>::value),
                            rhs.type());
    }
    return ComparisonImpl(lhs_value, rhs_value);
  }

  CelValue rhs;
};

}  // namespace

int CelValueCompare(CelValue lhs, CelValue rhs) {
  return lhs.InternalVisit<int>(ComparisonVisitor(rhs));
}

bool CelValueLessThan(CelValue lhs, CelValue rhs) {
  return lhs.InternalVisit<int>(ComparisonVisitor(rhs)) < 0;
}

bool CelValueEqual(CelValue lhs, CelValue rhs) {
  return lhs.InternalVisit<int>(ComparisonVisitor(rhs)) == 0;
}

bool CelValueGreaterThan(CelValue lhs, CelValue rhs) {
  return lhs.InternalVisit<int>(ComparisonVisitor(rhs)) > 0;
}

}  // namespace google::api::expr::runtime
