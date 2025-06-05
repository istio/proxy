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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_OBJECTWRITER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_OBJECTWRITER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/stubs/bytestream.h"
#include "google/protobuf/util/converter/structured_objectwriter.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// An ObjectWriter implementation that outputs JSON. This ObjectWriter
// supports writing a compact form or a pretty printed form.
//
// Sample usage:
//   string output;
//   StringOutputStream* str_stream = new StringOutputStream(&output);
//   CodedOutputStream* out_stream = new CodedOutputStream(str_stream);
//   JsonObjectWriter* ow = new JsonObjectWriter("  ", out_stream);
//   ow->StartObject("")
//       ->RenderString("name", "value")
//       ->RenderString("emptystring", string())
//       ->StartObject("nested")
//         ->RenderInt64("light", 299792458);
//         ->RenderDouble("pi", 3.141592653589793);
//       ->EndObject()
//       ->StartList("empty")
//       ->EndList()
//     ->EndObject();
//
// And then the output string would become:
// {
//   "name": "value",
//   "emptystring": "",
//   "nested": {
//     "light": "299792458",
//     "pi": 3.141592653589793
//   },
//   "empty": []
// }
//
// JsonObjectWriter does not validate if calls actually result in valid JSON.
// For example, passing an empty name when one would be required won't result
// in an error, just an invalid output.
//
// Note that all int64 and uint64 are rendered as strings instead of numbers.
// This is because JavaScript parses numbers as 64-bit float thus int64 and
// uint64 would lose precision if rendered as numbers.
//
// JsonObjectWriter is thread-unsafe.
class JsonObjectWriter : public StructuredObjectWriter {
 public:
  JsonObjectWriter(absl::string_view indent_string, io::CodedOutputStream* out)
      : element_(new Element(/*parent=*/nullptr, /*is_json_object=*/false)),
        stream_(out),
        sink_(out),
        indent_string_(indent_string),
        indent_char_('\0'),
        indent_count_(0),
        use_websafe_base64_for_bytes_(false) {
    // See if we have a trivial sequence of indent characters.
    if (!indent_string.empty()) {
      indent_char_ = indent_string[0];
      indent_count_ = indent_string.length();
      for (int i = 1; i < static_cast<int>(indent_string.length()); i++) {
        if (indent_char_ != indent_string_[i]) {
          indent_char_ = '\0';
          indent_count_ = 0;
          break;
        }
      }
    }
  }
  JsonObjectWriter(const JsonObjectWriter&) = delete;
  JsonObjectWriter& operator=(const JsonObjectWriter&) = delete;
  ~JsonObjectWriter() override;

  // ObjectWriter methods.
  JsonObjectWriter* StartObject(absl::string_view name) override;
  JsonObjectWriter* EndObject() override;
  JsonObjectWriter* StartList(absl::string_view name) override;
  JsonObjectWriter* EndList() override;
  JsonObjectWriter* RenderBool(absl::string_view name, bool value) override;
  JsonObjectWriter* RenderInt32(absl::string_view name, int32_t value) override;
  JsonObjectWriter* RenderUint32(absl::string_view name,
                                 uint32_t value) override;
  JsonObjectWriter* RenderInt64(absl::string_view name, int64_t value) override;
  JsonObjectWriter* RenderUint64(absl::string_view name,
                                 uint64_t value) override;
  JsonObjectWriter* RenderDouble(absl::string_view name, double value) override;
  JsonObjectWriter* RenderFloat(absl::string_view name, float value) override;
  JsonObjectWriter* RenderString(absl::string_view name,
                                 absl::string_view value) override;
  JsonObjectWriter* RenderBytes(absl::string_view name,
                                absl::string_view value) override;
  JsonObjectWriter* RenderNull(absl::string_view name) override;
  virtual JsonObjectWriter* RenderNullAsEmpty(absl::string_view name);

  void set_use_websafe_base64_for_bytes(bool value) {
    use_websafe_base64_for_bytes_ = value;
  }

 protected:
  class Element : public BaseElement {
   public:
    Element(Element* parent, bool is_json_object)
        : BaseElement(parent),
          is_first_(true),
          is_json_object_(is_json_object) {}
    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;

    // Called before each field of the Element is to be processed.
    // Returns true if this is the first call (processing the first field).
    bool is_first() {
      if (is_first_) {
        is_first_ = false;
        return true;
      }
      return false;
    }

    // Whether we are currently rendering inside a JSON object (i.e., between
    // StartObject() and EndObject()).
    bool is_json_object() const { return is_json_object_; }

   private:
    bool is_first_;
    bool is_json_object_;
  };

  Element* element() override { return element_.get(); }

 private:
  class ByteSinkWrapper : public strings::ByteSink {
   public:
    explicit ByteSinkWrapper(io::CodedOutputStream* stream) : stream_(stream) {}
    ByteSinkWrapper(const ByteSinkWrapper&) = delete;
    ByteSinkWrapper& operator=(const ByteSinkWrapper&) = delete;
    ~ByteSinkWrapper() override {}

    // ByteSink methods.
    void Append(const char* bytes, size_t n) override {
      stream_->WriteRaw(bytes, n);
    }

   private:
    io::CodedOutputStream* stream_;
  };

  // Renders a simple value as a string. By default all non-string Render
  // methods convert their argument to a string and call this method. This
  // method can then be used to render the simple value without escaping it.
  JsonObjectWriter* RenderSimple(absl::string_view name,
                                 absl::string_view value) {
    WritePrefix(name);
    WriteRawString(value);
    return this;
  }

  // Pushes a new JSON array element to the stack.
  void PushArray() {
    element_.reset(new Element(element_.release(), /*is_json_object=*/false));
  }

  // Pushes a new JSON object element to the stack.
  void PushObject() {
    element_.reset(new Element(element_.release(), /*is_json_object=*/true));
  }

  // Pops an element off of the stack and deletes the popped element.
  void Pop() {
    bool needs_newline = !element_->is_first();
    element_.reset(element_->pop<Element>());
    if (needs_newline) NewLine();
  }

  // If pretty printing is enabled, this will write a newline to the output,
  // followed by optional indentation. Otherwise this method is a noop.
  void NewLine() {
    if (!indent_string_.empty()) {
      size_t len = sizeof('\n') + (indent_string_.size() * element()->level());

      // Take the slow-path if we don't have sufficient characters remaining in
      // our buffer or we have a non-trivial indent string which would prevent
      // us from using memset.
      uint8_t* out = nullptr;
      if (indent_count_ > 0) {
        out = stream_->GetDirectBufferForNBytesAndAdvance(len);
      }

      if (out != nullptr) {
        out[0] = '\n';
        memset(&out[1], indent_char_, len - 1);
      } else {
        // Slow path, no contiguous output buffer available.
        WriteChar('\n');
        for (int i = 0; i < element()->level(); i++) {
          stream_->WriteRaw(indent_string_.c_str(), indent_string_.length());
        }
      }
    }
  }

  // Writes a prefix. This will write out any pretty printing and
  // commas that are required, followed by the name and a ':' if
  // the name is not null.
  void WritePrefix(absl::string_view name);

  // Writes an individual character to the output.
  void WriteChar(const char c) { stream_->WriteRaw(&c, sizeof(c)); }

  // Writes a string to the output.
  void WriteRawString(absl::string_view s) {
    stream_->WriteRaw(s.data(), s.length());
  }

  std::unique_ptr<Element> element_;
  io::CodedOutputStream* stream_;
  ByteSinkWrapper sink_;
  const std::string indent_string_;

  // For the common case of indent being a single character repeated.
  char indent_char_;
  int indent_count_;

  // Whether to use regular or websafe base64 encoding for byte fields. Defaults
  // to regular base64 encoding.
  bool use_websafe_base64_for_bytes_;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_OBJECTWRITER_H_
