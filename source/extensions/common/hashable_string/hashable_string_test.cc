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

#include "envoy/common/hashable.h"
#include "gmock/gmock.h"
#include "google/protobuf/repeated_field.h"
#include "gtest/gtest.h"
#include "source/extensions/filters/common/set_filter_state/filter_config.h"
#include "test/mocks/server/server_factory_context.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

#include <memory>
#include <vector>

namespace Istio {
namespace Common {
namespace {

TEST(HashableStringTest, TestHashableStringIsHashable) {
  using ::envoy::extensions::filters::common::set_filter_state::v3::FilterStateValue;
  using ::Envoy::Extensions::Filters::Common::SetFilterState::Config;
  using ::google::protobuf::RepeatedPtrField;

  NiceMock<Envoy::Server::Configuration::MockGenericFactoryContext> context;
  Envoy::Http::TestRequestHeaderMapImpl header_map;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;

  FilterStateValue proto;
  Envoy::TestUtility::loadFromYaml(R"YAML(
    object_key: key
    factory_key: istio.hashable_string
    format_string:
      text_format_source:
        inline_string: "value"
  )YAML",
                                   proto);
  std::vector<FilterStateValue> protos{{proto}};

  auto config =
      std::make_shared<Config>(RepeatedPtrField<FilterStateValue>(protos.begin(), protos.end()),
                               Envoy::StreamInfo::FilterState::LifeSpan::FilterChain, context);
  config->updateFilterState({&header_map}, stream_info);

  const auto* s = stream_info.filterState()->getDataReadOnly<Envoy::Hashable>("key");
  ASSERT_NE(s, nullptr);

  const HashableString* h = dynamic_cast<const HashableString*>(s);
  ASSERT_NE(h, nullptr);
  ASSERT_EQ(h->asString(), "value");
  ASSERT_EQ(h->serializeAsString(), "value");
}

} // namespace
} // namespace Common
} // namespace Istio
