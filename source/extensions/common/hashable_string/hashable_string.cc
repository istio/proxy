// Copyright Istio Authors. All Rights Reserved.
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
#include "source/extensions/common/hashable_string/hashable_string.h"

#include "envoy/registry/registry.h"
#include "envoy/stream_info/filter_state.h"

#include "source/common/common/hash.h"

#include <memory>
#include <string>

namespace Istio {
namespace Common {

HashableString::HashableString(std::string_view value) : Envoy::Router::StringAccessorImpl(value) {}

std::optional<uint64_t> HashableString::hash() const {
  return Envoy::HashUtil::xxHash64(asString());
}

namespace {

class HashableStringObjectFactory : public Envoy::StreamInfo::FilterState::ObjectFactory {
public:
  // ObjectFactory
  std::string name() const override { return "istio.hashable_string"; }

  std::unique_ptr<Envoy::StreamInfo::FilterState::Object>
  createFromBytes(std::string_view data) const override {
    return std::make_unique<HashableString>(data);
  }
};

REGISTER_FACTORY(HashableStringObjectFactory, Envoy::StreamInfo::FilterState::ObjectFactory);

} // namespace

} // namespace Common
} // namespace Istio
