/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_STRUCTURED_OBJECTWRITER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_STRUCTURED_OBJECTWRITER_H_

#include <memory>

#include "absl/base/casts.h"
#include "google/protobuf/util/converter/object_writer.h"
#include "google/protobuf/util/converter/port.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// An StructuredObjectWriter is an ObjectWriter for writing
// tree-structured data in a stream of events representing objects
// and collections. Implementation of this interface can be used to
// write an object stream to an in-memory structure, protobufs,
// JSON, XML, or any other output format desired. The ObjectSource
// interface is typically used as the source of an object stream.
//
// See JsonObjectWriter for a sample implementation of
// StructuredObjectWriter and its use.
//
// Derived classes could be thread-unsafe.
class StructuredObjectWriter : public ObjectWriter {
 public:
  StructuredObjectWriter(const StructuredObjectWriter&) = delete;
  StructuredObjectWriter& operator=(const StructuredObjectWriter&) = delete;
  ~StructuredObjectWriter() override {}

 protected:
  // A base element class for subclasses to extend, makes tracking state easier.
  //
  // StructuredObjectWriter behaves as a visitor. BaseElement represents a node
  // in the input tree. Implementation of StructuredObjectWriter should also
  // extend BaseElement to keep track of the location in the input tree.
  class BaseElement {
   public:
    // Takes ownership of the parent Element.
    explicit BaseElement(BaseElement* parent)
        : parent_(parent),
          level_(parent == nullptr ? 0 : parent->level() + 1) {}
    BaseElement() = delete;
    BaseElement(const BaseElement&) = delete;
    BaseElement& operator=(const BaseElement&) = delete;
    virtual ~BaseElement() {}

    // Releases ownership of the parent and returns a pointer to it.
    template <typename ElementType>
    ElementType* pop() {
      return proto_converter::internal::DownCast<ElementType*>(
          parent_.release());
    }

    // Returns true if this element is the root.
    bool is_root() const { return parent_ == nullptr; }

    // Returns the number of hops from this element to the root element.
    int level() const { return level_; }

   protected:
    // Returns pointer to parent element without releasing ownership.
    virtual BaseElement* parent() const { return parent_.get(); }

   private:
    // Pointer to the parent Element.
    std::unique_ptr<BaseElement> parent_;

    // Number of hops to the root Element.
    // The root Element has nullptr parent_ and a level_ of 0.
    const int level_;
  };

  StructuredObjectWriter() {}

  // Returns the current element. Used for indentation and name overrides.
  virtual BaseElement* element() = 0;

 private:
  // Do not add any data members to this class.
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_STRUCTURED_OBJECTWRITER_H_
