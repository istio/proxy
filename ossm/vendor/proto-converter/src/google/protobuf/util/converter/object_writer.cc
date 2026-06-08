// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/object_writer.h"

#include "google/protobuf/util/converter/datapiece.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// static
void ObjectWriter::RenderDataPieceTo(const DataPiece& data,
                                     absl::string_view name, ObjectWriter* ow) {
  switch (data.type()) {
    case DataPiece::TYPE_INT32: {
      ow->RenderInt32(name, data.ToInt32().value());
      break;
    }
    case DataPiece::TYPE_INT64: {
      ow->RenderInt64(name, data.ToInt64().value());
      break;
    }
    case DataPiece::TYPE_UINT32: {
      ow->RenderUint32(name, data.ToUint32().value());
      break;
    }
    case DataPiece::TYPE_UINT64: {
      ow->RenderUint64(name, data.ToUint64().value());
      break;
    }
    case DataPiece::TYPE_DOUBLE: {
      ow->RenderDouble(name, data.ToDouble().value());
      break;
    }
    case DataPiece::TYPE_FLOAT: {
      ow->RenderFloat(name, data.ToFloat().value());
      break;
    }
    case DataPiece::TYPE_BOOL: {
      ow->RenderBool(name, data.ToBool().value());
      break;
    }
    case DataPiece::TYPE_STRING: {
      ow->RenderString(name, data.ToString().value());
      break;
    }
    case DataPiece::TYPE_BYTES: {
      ow->RenderBytes(name, data.ToBytes().value());
      break;
    }
    case DataPiece::TYPE_NULL: {
      ow->RenderNull(name);
      break;
    }
    default:
      break;
  }
}


}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
