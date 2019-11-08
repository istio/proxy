/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "src/istio/mixerclient/attribute_compressor.h"

#include "google/protobuf/arena.h"
#include "include/istio/utils/protobuf.h"
#include "src/istio/mixerclient/global_dictionary.h"

using ::istio::mixer::v1::Attributes;
using ::istio::mixer::v1::Attributes_AttributeValue;
using ::istio::mixer::v1::Attributes_StringMap;
using ::istio::mixer::v1::CompressedAttributes;

namespace istio {
namespace mixerclient {
namespace {

// The size of first version of global dictionary.
// If any dictionary error, global dictionary will fall back to this version.
const int kGlobalDictionaryBaseSize = 111;

// Return per message dictionary index.
int MessageDictIndex(int idx) { return -(idx + 1); }

// Per message dictionary.
class MessageDictionary {
 public:
  MessageDictionary(const GlobalDictionary& global_dict)
      : global_dict_(global_dict) {}

  int GetIndex(const std::string& name) {
    int index;
    if (global_dict_.GetIndex(name, &index)) {
      return index;
    }

    const auto& message_it = message_dict_.find(name);
    if (message_it != message_dict_.end()) {
      return MessageDictIndex(message_it->second);
    }

    index = message_words_.size();
    message_words_.push_back(name);
    message_dict_[name] = index;
    return MessageDictIndex(index);
  }

  const std::vector<std::string>& GetWords() const { return message_words_; }

  void Clear() {
    message_words_.clear();
    message_dict_.clear();
  }

 private:
  const GlobalDictionary& global_dict_;

  // Per message dictionary
  std::vector<std::string> message_words_;
  std::unordered_map<std::string, int> message_dict_;
};

::istio::mixer::v1::StringMap CreateStringMap(
    const Attributes_StringMap& raw_map, MessageDictionary& dict) {
  ::istio::mixer::v1::StringMap compressed_map;
  auto* map_pb = compressed_map.mutable_entries();
  for (const auto& it : raw_map.entries()) {
    (*map_pb)[dict.GetIndex(it.first)] = dict.GetIndex(it.second);
  }
  return compressed_map;
}

void CompressByDict(const Attributes& attributes, MessageDictionary& dict,
                    CompressedAttributes* pb) {
  // Fill attributes.
  for (const auto& it : attributes.attributes()) {
    const std::string& name = it.first;
    const Attributes_AttributeValue& value = it.second;

    int index = dict.GetIndex(name);

    // Fill the attribute to proper map.
    switch (value.value_case()) {
      case Attributes_AttributeValue::kStringValue:
        (*pb->mutable_strings())[index] = dict.GetIndex(value.string_value());
        break;
      case Attributes_AttributeValue::kBytesValue:
        (*pb->mutable_bytes())[index] = value.bytes_value();
        break;
      case Attributes_AttributeValue::kInt64Value:
        (*pb->mutable_int64s())[index] = value.int64_value();
        break;
      case Attributes_AttributeValue::kDoubleValue:
        (*pb->mutable_doubles())[index] = value.double_value();
        break;
      case Attributes_AttributeValue::kBoolValue:
        (*pb->mutable_bools())[index] = value.bool_value();
        break;
      case Attributes_AttributeValue::kTimestampValue:
        (*pb->mutable_timestamps())[index] = value.timestamp_value();
        break;
      case Attributes_AttributeValue::kDurationValue:
        (*pb->mutable_durations())[index] = value.duration_value();
        break;
      case Attributes_AttributeValue::kStringMapValue:
        (*pb->mutable_string_maps())[index] =
            CreateStringMap(value.string_map_value(), dict);
        break;
      case Attributes_AttributeValue::VALUE_NOT_SET:
        break;
    }
  }
}

class BatchCompressorImpl : public BatchCompressor {
 public:
  BatchCompressorImpl(const GlobalDictionary& global_dict)
      : global_dict_(global_dict), dict_(global_dict) {}

  void Add(const Attributes& attributes) override {
    CompressByDict(attributes, dict_, report_.add_attributes());
  }

  int size() const override { return report_.attributes_size(); }

  const ::istio::mixer::v1::ReportRequest& Finish() override {
    for (const std::string& word : dict_.GetWords()) {
      report_.add_default_words(word);
    }
    report_.set_global_word_count(global_dict_.size());
    report_.set_repeated_attributes_semantics(
        mixer::v1::
            ReportRequest_RepeatedAttributesSemantics_INDEPENDENT_ENCODING);
    return report_;
  }

  void Clear() override {
    dict_.Clear();
    report_.Clear();
  }

 private:
  const GlobalDictionary& global_dict_;
  MessageDictionary dict_;
  ::istio::mixer::v1::ReportRequest report_;
};

}  // namespace

GlobalDictionary::GlobalDictionary() {
  const std::vector<std::string>& global_words = GetGlobalWords();
  for (unsigned int i = 0; i < global_words.size(); i++) {
    global_dict_[global_words[i]] = i;
  }
  top_index_ = global_words.size();
}

// Lookup the index, return true if found.
bool GlobalDictionary::GetIndex(const std::string name, int* index) const {
  const auto& it = global_dict_.find(name);
  if (it != global_dict_.end() && it->second < top_index_) {
    // Return global dictionary index.
    *index = it->second;
    return true;
  }
  return false;
}

void GlobalDictionary::ShrinkToBase() {
  if (top_index_ > kGlobalDictionaryBaseSize) {
    top_index_ = kGlobalDictionaryBaseSize;
    GOOGLE_LOG(INFO) << "Shrink global dictionary " << top_index_
                     << " to base.";
  }
}

void AttributeCompressor::Compress(
    const Attributes& attributes,
    ::istio::mixer::v1::CompressedAttributes* pb) const {
  MessageDictionary dict(global_dict_);
  CompressByDict(attributes, dict, pb);

  for (const std::string& word : dict.GetWords()) {
    pb->add_words(word);
  }
}

std::unique_ptr<BatchCompressor> AttributeCompressor::CreateBatchCompressor()
    const {
  return std::unique_ptr<BatchCompressor>(
      new BatchCompressorImpl(global_dict_));
}

}  // namespace mixerclient
}  // namespace istio
