// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "grpc_transcoding/prefix_writer.h"

#include <cstdint>
#include <string>

#include "absl/strings/str_split.h"
#include "google/protobuf/util/converter/object_writer.h"

namespace google {
namespace grpc {

namespace transcoding {

PrefixWriter::PrefixWriter(const std::string& prefix,
                           google::protobuf::util::converter::ObjectWriter* ow)
    : prefix_(absl::StrSplit(prefix, ".", absl::SkipEmpty())),
      non_actionable_depth_(0),
      writer_(ow) {}

PrefixWriter* PrefixWriter::StartObject(absl::string_view name) {
  if (++non_actionable_depth_ == 1) {
    name = StartPrefix(name);
  }
  writer_->StartObject(name);
  return this;
}

PrefixWriter* PrefixWriter::EndObject() {
  writer_->EndObject();
  if (--non_actionable_depth_ == 0) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::StartList(absl::string_view name) {
  if (++non_actionable_depth_ == 1) {
    name = StartPrefix(name);
  }
  writer_->StartList(name);
  return this;
}

PrefixWriter* PrefixWriter::EndList() {
  writer_->EndList();
  if (--non_actionable_depth_ == 0) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderBool(absl::string_view name, bool value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderBool(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderInt32(absl::string_view name, int32_t value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderInt32(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderUint32(absl::string_view name,
                                         uint32_t value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderUint32(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderInt64(absl::string_view name, int64_t value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderInt64(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderUint64(absl::string_view name,
                                         uint64_t value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderUint64(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderDouble(absl::string_view name, double value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderDouble(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderFloat(absl::string_view name, float value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderFloat(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderString(absl::string_view name,
                                         absl::string_view value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderString(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderBytes(absl::string_view name,
                                        absl::string_view value) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }
  writer_->RenderBytes(name, value);
  if (root) {
    EndPrefix();
  }
  return this;
}

PrefixWriter* PrefixWriter::RenderNull(absl::string_view name) {
  bool root = non_actionable_depth_ == 0;
  if (root) {
    name = StartPrefix(name);
  }

  writer_->RenderNull(name);
  if (root) {
    EndPrefix();
  }
  return this;
}

absl::string_view PrefixWriter::StartPrefix(absl::string_view name) {
  for (const auto& prefix : prefix_) {
    writer_->StartObject(name);
    name = prefix;
  }
  return name;
}

void PrefixWriter::EndPrefix() {
  for (size_t i = 0; i < prefix_.size(); ++i) {
    writer_->EndObject();
  }
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
