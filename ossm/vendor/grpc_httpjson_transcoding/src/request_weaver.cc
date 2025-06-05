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
#include "grpc_transcoding/request_weaver.h"

#include <cmath>
#include <cstdint>
#include <cfloat>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/stubs/strutil.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/datapiece.h"
#include "google/protobuf/util/converter/object_writer.h"
#include "google/protobuf/util/field_comparator.h"

namespace google {
namespace grpc {

namespace transcoding {

namespace pb = google::protobuf;
namespace pbconv = google::protobuf::util::converter;

namespace {

bool AlmostEquals(float a, float b) { return fabs(a - b) < 32 * FLT_EPSILON; }

absl::Status bindingFailureStatus(absl::string_view field_name,
                                  absl::string_view type,
                                  const pbconv::DataPiece& value) {
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Failed to convert binding value ", field_name, ":",
                   value.ValueAsStringOrDefault(""), " to ", type));
}

absl::Status isEqual(absl::string_view field_name,
                     const pbconv::DataPiece& value_in_body,
                     const pbconv::DataPiece& value_in_binding) {
  bool value_is_same = true;
  switch (value_in_body.type()) {
    case pbconv::DataPiece::TYPE_INT32: {
      absl::StatusOr<int32_t> status = value_in_binding.ToInt32();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "int32", value_in_binding);
      }
      if (status.value() != value_in_body.ToInt32().value()) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_INT64: {
      absl::StatusOr<uint32_t> status = value_in_binding.ToInt64();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "int64", value_in_binding);
      }
      if (status.value() != value_in_body.ToInt64().value()) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_UINT32: {
      absl::StatusOr<uint32_t> status = value_in_binding.ToUint32();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "uint32", value_in_binding);
      }
      if (status.value() != value_in_body.ToUint32().value()) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_UINT64: {
      absl::StatusOr<uint32_t> status = value_in_binding.ToUint64();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "uint64", value_in_binding);
      }
      if (status.value() != value_in_body.ToUint64().value()) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_DOUBLE: {
      absl::StatusOr<double> status = value_in_binding.ToDouble();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "double", value_in_binding);
      }
      if (!AlmostEquals(status.value(), value_in_body.ToDouble().value())) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_FLOAT: {
      absl::StatusOr<float> status = value_in_binding.ToFloat();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "float", value_in_binding);
      }
      if (!AlmostEquals(status.value(), value_in_body.ToFloat().value())) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_BOOL: {
      absl::StatusOr<bool> status = value_in_binding.ToBool();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "bool", value_in_binding);
      }
      if (status.value() != value_in_body.ToBool().value()) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_STRING: {
      absl::StatusOr<std::string> status = value_in_binding.ToString();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "string", value_in_binding);
      }
      if (status.value() != value_in_body.ToString().value()) {
        value_is_same = false;
      }
      break;
    }
    case pbconv::DataPiece::TYPE_BYTES: {
      absl::StatusOr<std::string> status = value_in_binding.ToBytes();
      if (!status.ok()) {
        return bindingFailureStatus(field_name, "bytes", value_in_binding);
      }
      if (status.value() != value_in_body.ToBytes().value()) {
        value_is_same = false;
      }
      break;
    }
    default:
      break;
  }
  if (!value_is_same) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("The binding value %s of the field %s is "
                        "conflicting with the value %s in the body.",
                        value_in_binding.ValueAsStringOrDefault(""),
                        std::string(field_name),
                        value_in_body.ValueAsStringOrDefault("")));
  }
  return absl::OkStatus();
}

}  // namespace

RequestWeaver::RequestWeaver(std::vector<BindingInfo> bindings,
                             pbconv::ObjectWriter* ow, StatusErrorListener* el,
                             bool report_collisions)
    : root_(),
      current_(),
      ow_(ow),
      non_actionable_depth_(0),
      error_listener_(el),
      report_collisions_(report_collisions) {
  for (const auto& b : bindings) {
    Bind(std::move(b.field_path), std::move(b.value));
  }
}

RequestWeaver* RequestWeaver::StartObject(absl::string_view name) {
  ow_->StartObject(name);
  if (current_.empty()) {
    // The outermost StartObject("");
    current_.push(&root_);
    return this;
  }
  if (non_actionable_depth_ == 0) {
    WeaveInfo* info = current_.top()->FindWeaveMsg(name);
    if (info != nullptr) {
      current_.push(info);
      return this;
    }
  }
  // At this point, we don't match any messages we need to weave into, so
  // we won't need to do any matching until we leave this object.
  ++non_actionable_depth_;
  return this;
}

RequestWeaver* RequestWeaver::EndObject() {
  if (non_actionable_depth_ > 0) {
    --non_actionable_depth_;
  } else {
    WeaveTree(current_.top());
    current_.pop();
  }
  ow_->EndObject();
  return this;
}

RequestWeaver* RequestWeaver::StartList(absl::string_view name) {
  ow_->StartList(name);
  // We don't support weaving inside lists, so we won't need to do any matching
  // until we leave this list.
  ++non_actionable_depth_;
  return this;
}

RequestWeaver* RequestWeaver::EndList() {
  ow_->EndList();
  --non_actionable_depth_;
  return this;
}

RequestWeaver* RequestWeaver::RenderBool(absl::string_view name, bool value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderBool(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderInt32(absl::string_view name,
                                          int32_t value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderInt32(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderUint32(absl::string_view name,
                                           uint32_t value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderUint32(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderInt64(absl::string_view name,
                                          int64_t value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderInt64(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderUint64(absl::string_view name,
                                           uint64_t value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderUint64(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderDouble(absl::string_view name,
                                           double value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderDouble(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderFloat(absl::string_view name, float value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderFloat(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderString(absl::string_view name,
                                           absl::string_view value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value, true);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderString(name, value);
  return this;
}

RequestWeaver* RequestWeaver::RenderNull(absl::string_view name) {
  ow_->RenderNull(name);
  return this;
}

RequestWeaver* RequestWeaver::RenderBytes(absl::string_view name,
                                          absl::string_view value) {
  if (non_actionable_depth_ == 0) {
    pbconv::DataPiece value_in_body = pbconv::DataPiece(value, true);
    CollisionCheck(name, value_in_body);
  }
  ow_->RenderBytes(name, value);
  return this;
}

void RequestWeaver::Bind(std::vector<const pb::Field*> field_path,
                         std::string value) {
  WeaveInfo* current = &root_;

  // Find or create the path from the root to the leaf message, where the value
  // should be injected.
  for (size_t i = 0; i < field_path.size() - 1; ++i) {
    current = current->FindOrCreateWeaveMsg(field_path[i]);
  }

  if (!field_path.empty()) {
    current->bindings.emplace_back(field_path.back(), std::move(value));
  }
}

void RequestWeaver::WeaveTree(RequestWeaver::WeaveInfo* info) {
  for (const auto& data : info->bindings) {
    pbconv::ObjectWriter::RenderDataPieceTo(
        pbconv::DataPiece(absl::string_view(data.second), true),
        absl::string_view(data.first->name()), ow_);
  }
  info->bindings.clear();
  for (auto& msg : info->messages) {
    // Enter into the message only if there are bindings or submessages left
    if (!msg.second.bindings.empty() || !msg.second.messages.empty()) {
      ow_->StartObject(msg.first->name());
      WeaveTree(&msg.second);
      ow_->EndObject();
    }
  }
  info->messages.clear();
}

void RequestWeaver::CollisionCheck(absl::string_view name,
                                   const pbconv::DataPiece& value_in_body) {
  if (current_.empty()) return;

  for (auto it = current_.top()->bindings.begin();
       it != current_.top()->bindings.end();) {
    if (name == it->first->name()) {
      if (it->first->cardinality() == pb::Field::CARDINALITY_REPEATED) {
        pbconv::ObjectWriter::RenderDataPieceTo(
            pbconv::DataPiece(absl::string_view(it->second), true), name, ow_);
      } else if (report_collisions_) {
        pbconv::DataPiece value_in_binding =
            pbconv::DataPiece(absl::string_view(it->second), true);
        absl::Status compare_status =
            isEqual(name, value_in_body, value_in_binding);
        if (!compare_status.ok()) {
          error_listener_->set_status(compare_status);
        }
      }
      it = current_.top()->bindings.erase(it);
      continue;
    }
    ++it;
  }
}

RequestWeaver::WeaveInfo* RequestWeaver::WeaveInfo::FindWeaveMsg(
    const absl::string_view field_name) {
  for (auto& msg : messages) {
    if (field_name == msg.first->name()) {
      return &msg.second;
    }
  }
  return nullptr;
}

RequestWeaver::WeaveInfo* RequestWeaver::WeaveInfo::CreateWeaveMsg(
    const pb::Field* field) {
  messages.emplace_back(field, WeaveInfo());
  return &messages.back().second;
}

RequestWeaver::WeaveInfo* RequestWeaver::WeaveInfo::FindOrCreateWeaveMsg(
    const pb::Field* field) {
  WeaveInfo* found = FindWeaveMsg(field->name());
  return found == nullptr ? CreateWeaveMsg(field) : found;
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
