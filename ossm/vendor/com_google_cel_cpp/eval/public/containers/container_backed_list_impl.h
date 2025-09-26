#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_LIST_IMPL_H_

#include <utility>
#include <vector>

#include "eval/public/cel_value.h"
#include "google/protobuf/arena.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelList implementation that uses "repeated" message field
// as backing storage.
class ContainerBackedListImpl : public CelList {
 public:
  // message contains the "repeated" field
  // descriptor FieldDescriptor for the field
  explicit ContainerBackedListImpl(std::vector<CelValue> values)
      : values_(std::move(values)) {}

  // List size.
  int size() const override { return values_.size(); }

  // List element access operator.
  CelValue operator[](int index) const override { return values_[index]; }

  // List element access operator.
  CelValue Get(google::protobuf::Arena*, int index) const override {
    return values_[index];
  }

 private:
  std::vector<CelValue> values_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_LIST_IMPL_H_
