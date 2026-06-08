#include "eval/public/value_export_util.h"

#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "internal/proto_time_encoding.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/time_util.h"

namespace google::api::expr::runtime {

using google::protobuf::Duration;
using google::protobuf::Timestamp;
using google::protobuf::Value;
using google::protobuf::util::TimeUtil;

absl::Status KeyAsString(const CelValue& value, std::string* key) {
  switch (value.type()) {
    case CelValue::Type::kInt64: {
      *key = absl::StrCat(value.Int64OrDie());
      break;
    }
    case CelValue::Type::kUint64: {
      *key = absl::StrCat(value.Uint64OrDie());
      break;
    }
    case CelValue::Type::kString: {
      key->assign(value.StringOrDie().value().data(),
                  value.StringOrDie().value().size());
      break;
    }
    default: {
      return absl::InvalidArgumentError("Unsupported map type");
    }
  }
  return absl::OkStatus();
}

//  Export content of CelValue as google.protobuf.Value.
absl::Status ExportAsProtoValue(const CelValue& in_value, Value* out_value,
                                google::protobuf::Arena* arena) {
  if (in_value.IsNull()) {
    out_value->set_null_value(google::protobuf::NULL_VALUE);
    return absl::OkStatus();
  }
  switch (in_value.type()) {
    case CelValue::Type::kBool: {
      out_value->set_bool_value(in_value.BoolOrDie());
      break;
    }
    case CelValue::Type::kInt64: {
      out_value->set_number_value(static_cast<double>(in_value.Int64OrDie()));
      break;
    }
    case CelValue::Type::kUint64: {
      out_value->set_number_value(static_cast<double>(in_value.Uint64OrDie()));
      break;
    }
    case CelValue::Type::kDouble: {
      out_value->set_number_value(in_value.DoubleOrDie());
      break;
    }
    case CelValue::Type::kString: {
      auto value = in_value.StringOrDie().value();
      out_value->set_string_value(value.data(), value.size());
      break;
    }
    case CelValue::Type::kBytes: {
      absl::Base64Escape(in_value.BytesOrDie().value(),
                         out_value->mutable_string_value());
      break;
    }
    case CelValue::Type::kDuration: {
      Duration duration;
      auto status =
          cel::internal::EncodeDuration(in_value.DurationOrDie(), &duration);
      if (!status.ok()) {
        return status;
      }
      out_value->set_string_value(TimeUtil::ToString(duration));
      break;
    }
    case CelValue::Type::kTimestamp: {
      Timestamp timestamp;
      auto status =
          cel::internal::EncodeTime(in_value.TimestampOrDie(), &timestamp);
      if (!status.ok()) {
        return status;
      }
      out_value->set_string_value(TimeUtil::ToString(timestamp));
      break;
    }
    case CelValue::Type::kMessage: {
      google::protobuf::util::JsonPrintOptions json_options;
      json_options.preserve_proto_field_names = true;
      std::string json;
      auto status = google::protobuf::util::MessageToJsonString(*in_value.MessageOrDie(),
                                                      &json, json_options);
      if (!status.ok()) {
        return absl::InternalError(status.ToString());
      }
      google::protobuf::util::JsonParseOptions json_parse_options;
      status = google::protobuf::util::JsonStringToMessage(json, out_value,
                                                 json_parse_options);
      if (!status.ok()) {
        return absl::InternalError(status.ToString());
      }
      break;
    }
    case CelValue::Type::kList: {
      const CelList* cel_list = in_value.ListOrDie();
      auto out_values = out_value->mutable_list_value();
      for (int i = 0; i < cel_list->size(); i++) {
        auto status = ExportAsProtoValue((*cel_list).Get(arena, i),
                                         out_values->add_values(), arena);
        if (!status.ok()) {
          return status;
        }
      }
      break;
    }
    case CelValue::Type::kMap: {
      const CelMap* cel_map = in_value.MapOrDie();
      CEL_ASSIGN_OR_RETURN(auto keys_list, cel_map->ListKeys(arena));
      auto out_values = out_value->mutable_struct_value()->mutable_fields();
      for (int i = 0; i < keys_list->size(); i++) {
        std::string key;
        CelValue map_key = (*keys_list).Get(arena, i);
        auto status = KeyAsString(map_key, &key);
        if (!status.ok()) {
          return status;
        }
        auto map_value_ref = (*cel_map).Get(arena, map_key);
        CelValue map_value =
            (map_value_ref) ? map_value_ref.value() : CelValue();
        status = ExportAsProtoValue(map_value, &((*out_values)[key]), arena);
        if (!status.ok()) {
          return status;
        }
      }
      break;
    }
    default: {
      return absl::InvalidArgumentError("Unsupported value type");
    }
  }
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
