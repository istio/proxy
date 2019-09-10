/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NULL_PLUGIN
#include "plugin.h"
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

// clang-format off
constexpr char CHAR_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr unsigned char REVERSE_LOOKUP_TABLE[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

class Base64 {
 public:
  static std::string encode(const char* input, uint64_t length,
                            bool add_padding);
  static std::string encode(const char* input, uint64_t length) {
    return encode(input, length, true);
  }
  static std::string decodeWithoutPadding(absl::string_view input);
};

inline bool decodeBase(const uint8_t cur_char, uint64_t pos, std::string& ret,
                       const unsigned char* const reverse_lookup_table) {
  const unsigned char c = reverse_lookup_table[static_cast<uint32_t>(cur_char)];
  if (c == 64) {
    // Invalid character
    return false;
  }

  switch (pos % 4) {
  case 0:
    ret.push_back(c << 2);
    break;
  case 1:
    ret.back() |= c >> 4;
    ret.push_back(c << 4);
    break;
  case 2:
    ret.back() |= c >> 2;
    ret.push_back(c << 6);
    break;
  case 3:
    ret.back() |= c;
    break;
  }
  return true;
}

inline bool decodeLast(const uint8_t cur_char, uint64_t pos, std::string& ret,
                       const unsigned char* const reverse_lookup_table) {
  const unsigned char c = reverse_lookup_table[static_cast<uint32_t>(cur_char)];
  if (c == 64) {
    // Invalid character
    return false;
  }

  switch (pos % 4) {
  case 0:
    return false;
  case 1:
    ret.back() |= c >> 4;
    return (c & 0b1111) == 0;
  case 2:
    ret.back() |= c >> 2;
    return (c & 0b11) == 0;
  case 3:
    ret.back() |= c;
    break;
  }
  return true;
}

inline void encodeBase(const uint8_t cur_char, uint64_t pos, uint8_t& next_c, std::string& ret,
                       const char* const char_table) {
  switch (pos % 3) {
  case 0:
    ret.push_back(char_table[cur_char >> 2]);
    next_c = (cur_char & 0x03) << 4;
    break;
  case 1:
    ret.push_back(char_table[next_c | (cur_char >> 4)]);
    next_c = (cur_char & 0x0f) << 2;
    break;
  case 2:
    ret.push_back(char_table[next_c | (cur_char >> 6)]);
    ret.push_back(char_table[cur_char & 0x3f]);
    next_c = 0;
    break;
  }
}

inline void encodeLast(uint64_t pos, uint8_t last_char, std::string& ret,
                       const char* const char_table, bool add_padding) {
  switch (pos % 3) {
  case 1:
    ret.push_back(char_table[last_char]);
    if (add_padding) {
      ret.push_back('=');
      ret.push_back('=');
    }
    break;
  case 2:
    ret.push_back(char_table[last_char]);
    if (add_padding) {
      ret.push_back('=');
    }
    break;
  default:
    break;
  }
}

std::string Base64::encode(const char* input, uint64_t length,
                           bool add_padding) {
  uint64_t output_length = (length + 2) / 3 * 4;
  std::string ret;
  ret.reserve(output_length);

  uint64_t pos = 0;
  uint8_t next_c = 0;

  for (uint64_t i = 0; i < length; ++i) {
    encodeBase(input[i], pos++, next_c, ret, CHAR_TABLE);
  }

  encodeLast(pos, next_c, ret, CHAR_TABLE, add_padding);

  return ret;
}

std::string Base64::decodeWithoutPadding(absl::string_view input) {
  if (input.empty()) {
    return EMPTY_STRING;
  }

  // At most last two chars can be '='.
  size_t n = input.length();
  if (input[n - 1] == '=') {
    n--;
    if (n > 0 && input[n - 1] == '=') {
      n--;
    }
  }
  // Last position before "valid" padding character.
  uint64_t last = n - 1;
  // Determine output length.
  size_t max_length = (n + 3) / 4 * 3;
  if (n % 4 == 3) {
    max_length -= 1;
  }
  if (n % 4 == 2) {
    max_length -= 2;
  }

  std::string ret;
  ret.reserve(max_length);
  for (uint64_t i = 0; i < last; ++i) {
    if (!decodeBase(input[i], i, ret, REVERSE_LOOKUP_TABLE)) {
      return EMPTY_STRING;
    }
  }

  if (!decodeLast(input[last], last, ret, REVERSE_LOOKUP_TABLE)) {
    return EMPTY_STRING;
  }

  ASSERT(ret.size() == max_length);
  return ret;
}

#else

#include "common/common/base64.h"
#include "src/envoy/http/metadata_exchange/plugin.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {
namespace Plugin {

using namespace ::Envoy::Extensions::Common::Wasm::Null::Plugin;
using NullPluginRootRegistry =
  ::Envoy::Extensions::Common::Wasm::Null::NullPluginRootRegistry;

NULL_PLUGIN_ROOT_REGISTRY;

#endif

namespace {

bool serializeToStringDeterministic(const google::protobuf::Struct& metadata,
                                    std::string* metadata_bytes) {
  google::protobuf::io::StringOutputStream md(metadata_bytes);
  google::protobuf::io::CodedOutputStream mcs(&md);

  mcs.SetSerializationDeterministic(true);
  if (!metadata.SerializeToCodedStream(&mcs)) {
    logWarn("unable to serialize metadata");
    return false;
  }
  return true;
}

}  // namespace

static RegisterContextFactory register_MetadataExchange(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

void PluginRootContext::updateMetadataValue() {
  google::protobuf::Value keys_value;
  if (getMetadataValue(MetadataType::Node,
                       NodeMetadataExchangeKeys,
                       &keys_value) != WasmResult::Ok) {
    logDebug(
        absl::StrCat("cannot get metadata key: ", NodeMetadataExchangeKeys));
    return;
  }

  if (keys_value.kind_case() != google::protobuf::Value::kStringValue) {
    logWarn(absl::StrCat("metadata key is not a string: ",
                         NodeMetadataExchangeKeys));
    return;
  }

  google::protobuf::Struct metadata;

  // select keys from the metadata using the keys
  const std::set<absl::string_view> keys =
      absl::StrSplit(keys_value.string_value(), ',', absl::SkipWhitespace());
  for (auto key : keys) {
    google::protobuf::Value value;
    if (getMetadataValue(MetadataType::Node, key, &value) ==
        WasmResult::Ok) {
      (*metadata.mutable_fields())[std::string(key)] = value;
    } else {
      logDebug(absl::StrCat("cannot get metadata key: ", key));
    }
  }

  // store serialized form
  std::string metadata_bytes;
  serializeToStringDeterministic(metadata, &metadata_bytes);
  metadata_value_ =
      Base64::encode(metadata_bytes.data(), metadata_bytes.size());
}

void PluginRootContext::onConfigure(
    std::unique_ptr<WasmData> ABSL_ATTRIBUTE_UNUSED configuration) {
  updateMetadataValue();

  // TODO: this is really expensive since it fetches the entire metadata from
  // before magic "." to get the whole node.
  google::protobuf::Struct node;
  if (getMetadataStruct(MetadataType::Node, WholeNodeKey,
                        &node) == WasmResult::Ok) {
    for (const auto& f : node.fields()) {
      if (f.first == NodeIdKey &&
          f.second.kind_case() == google::protobuf::Value::kStringValue) {
        node_id_ = f.second.string_value();
        break;
      }
    }
  } else {
    logDebug(absl::StrCat("cannot get metadata key: ", WholeNodeKey));
  }

  logDebug(
      absl::StrCat("metadata_value_ id:", id(), " value:", metadata_value_));
}

FilterHeadersStatus PluginContext::onRequestHeaders() {
  // strip and store downstream peer metadata
  auto downstream_metadata_value = getRequestHeader(ExchangeMetadataHeader);
  if (downstream_metadata_value != nullptr &&
      !downstream_metadata_value->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeader);
    auto downstream_metadata_bytes =
        Base64::decodeWithoutPadding(downstream_metadata_value->view());
    setMetadataStruct(MetadataType::Request,
                      DownstreamMetadataKey, downstream_metadata_bytes);
  }

  auto downstream_metadata_id = getRequestHeader(ExchangeMetadataHeaderId);
  if (downstream_metadata_id != nullptr &&
      !downstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setMetadataStringValue(MetadataType::Request,
                           DownstreamMetadataIdKey,
                           downstream_metadata_id->view());
  }

  auto metadata = metadataValue();
  // insert peer metadata struct for upstream
  if (!metadata.empty()) {
    replaceRequestHeader(ExchangeMetadataHeader, metadata);
  }

  auto nodeid = nodeId();
  if (!nodeid.empty()) {
    replaceRequestHeader(ExchangeMetadataHeaderId, nodeid);
  }

  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus PluginContext::onResponseHeaders() {
  // strip and store upstream peer metadata
  auto upstream_metadata_value = getResponseHeader(ExchangeMetadataHeader);
  if (upstream_metadata_value != nullptr &&
      !upstream_metadata_value->view().empty()) {
    removeResponseHeader(ExchangeMetadataHeader);
    auto upstream_metadata_bytes =
        Base64::decodeWithoutPadding(upstream_metadata_value->view());
    setMetadataStruct(MetadataType::Request, UpstreamMetadataKey,
                      upstream_metadata_bytes);
  }

  auto upstream_metadata_id = getResponseHeader(ExchangeMetadataHeaderId);
  if (upstream_metadata_id != nullptr &&
      !upstream_metadata_id->view().empty()) {
    removeRequestHeader(ExchangeMetadataHeaderId);
    setMetadataStringValue(MetadataType::Request,
                           UpstreamMetadataIdKey, upstream_metadata_id->view());
  }

  auto metadata = metadataValue();
  // insert peer metadata struct for downstream
  if (!metadata.empty()) {
    replaceResponseHeader(ExchangeMetadataHeader, metadata);
  }

  auto nodeid = nodeId();
  if (!nodeid.empty()) {
    replaceResponseHeader(ExchangeMetadataHeaderId, nodeid);
  }

  return FilterHeadersStatus::Continue;
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
#endif
