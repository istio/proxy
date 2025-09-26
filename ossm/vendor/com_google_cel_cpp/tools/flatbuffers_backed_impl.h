#ifndef THIRD_PARTY_CEL_CPP_TOOLS_FLATBUFFERS_BACKED_IMPL_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_FLATBUFFERS_BACKED_IMPL_H_

#include "eval/public/cel_value.h"
#include "flatbuffers/reflection.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class FlatBuffersMapImpl : public CelMap {
 public:
  FlatBuffersMapImpl(const flatbuffers::Table& table,
                     const reflection::Schema& schema,
                     const reflection::Object& object, google::protobuf::Arena* arena)
      : arena_(arena), table_(table), schema_(schema) {
    keys_.fields = object.fields();
  }

  int size() const override { return keys_.fields->size(); }

  absl::StatusOr<bool> Has(const CelValue& key) const override;

  absl::optional<CelValue> operator[](CelValue cel_key) const override;

  // Import base class signatures to bypass GCC warning/error.
  using CelMap::ListKeys;
  absl::StatusOr<const CelList*> ListKeys() const override { return &keys_; }

 private:
  struct FieldList : public CelList {
    int size() const override { return fields->size(); }
    CelValue operator[](int index) const override {
      auto name = fields->Get(index)->name();
      return CelValue::CreateStringView(
          absl::string_view(name->c_str(), name->size()));
    }
    const flatbuffers::Vector<flatbuffers::Offset<reflection::Field>>* fields;
  };
  FieldList keys_;
  google::protobuf::Arena* arena_;
  const flatbuffers::Table& table_;
  const reflection::Schema& schema_;
};

// Factory method to instantiate a CelValue on the arena for flatbuffer object
// from a reflection schema.
const CelMap* CreateFlatBuffersBackedObject(const uint8_t* flatbuf,
                                            const reflection::Schema& schema,
                                            google::protobuf::Arena* arena);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_FLATBUFFERS_BACKED_IMPL_H_
