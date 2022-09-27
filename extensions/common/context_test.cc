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

#include "extensions/common/context.h"

#include "gtest/gtest.h"

namespace Wasm {
namespace Common {
namespace {

TEST(Context, WorkloadMetadataObjectConverson) {
  const std::string baggage =
      "k8s.cluster.name=my-cluster,"
      "k8s.namespace.name=default,k8s.pod.name=foo,"
      "service.name=foo-service,service.version=v1alpha3,"
      "app.name=foo-app,app.version=v1";
  auto obj = Istio::Common::WorkloadMetadataObject::fromBaggage(baggage);
  auto buffer = convertWorkloadMetadataToFlatNode(obj);
  const auto& node =
      *flatbuffers::GetRoot<Wasm::Common::FlatNode>(buffer.data());
  auto obj2 = convertFlatNodeToWorkloadMetadata(node);
  EXPECT_EQ(obj2.baggage(), baggage);
}

}  // namespace
}  // namespace Common
}  // namespace Wasm
