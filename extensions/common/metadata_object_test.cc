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

#include "extensions/common/metadata_object.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Istio {
namespace Common {

using ::testing::NiceMock;

TEST(WorkloadMetadataObjectTest, Hash) {
  WorkloadMetadataObject obj1("foo-pod-12345", "my-cluster", "default", "foo", "foo", "latest",
                              "foo-app", "v1", WorkloadType::Deployment);
  WorkloadMetadataObject obj2("foo-pod-12345", "my-cluster", "default", "bar", "baz", "first",
                              "foo-app", "v1", WorkloadType::Job);

  EXPECT_EQ(obj1.hash().value(), obj2.hash().value());
}

TEST(WorkloadMetadataObjectTest, Baggage) {
  WorkloadMetadataObject deploy("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                                "v1alpha3", "foo-app", "v1", WorkloadType::Deployment);

  WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                             "v1alpha3", "foo-app", "v1", WorkloadType::Pod);

  WorkloadMetadataObject cronjob("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                                 "v1alpha3", "foo-app", "v1", WorkloadType::CronJob);

  WorkloadMetadataObject job("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                             "v1alpha3", "foo-app", "v1", WorkloadType::Job);

  EXPECT_EQ(deploy.baggage(), absl::StrCat("k8s.deployment.name=foo,k8s.cluster.name=my-cluster,",
                                           "k8s.namespace.name=default,",
                                           "service.name=foo-service,service.version=v1alpha3,",
                                           "app.name=foo-app,app.version=v1"));

  EXPECT_EQ(pod.baggage(), absl::StrCat("k8s.pod.name=foo,k8s.cluster.name=my-cluster,",
                                        "k8s.namespace.name=default,",
                                        "service.name=foo-service,service.version=v1alpha3,",
                                        "app.name=foo-app,app.version=v1"));

  EXPECT_EQ(cronjob.baggage(), absl::StrCat("k8s.cronjob.name=foo,k8s.cluster.name=my-cluster,",
                                            "k8s.namespace.name=default,"
                                            "service.name=foo-service,service.version=v1alpha3,",
                                            "app.name=foo-app,app.version=v1"));

  EXPECT_EQ(job.baggage(), absl::StrCat("k8s.job.name=foo,k8s.cluster.name=my-cluster,",
                                        "k8s.namespace.name=default,",
                                        "service.name=foo-service,service.version=v1alpha3,",
                                        "app.name=foo-app,app.version=v1"));
}

void checkFlatNodeConversion(const WorkloadMetadataObject& obj) {
  auto buffer = convertWorkloadMetadataToFlatNode(obj);
  const auto& node = *flatbuffers::GetRoot<Wasm::Common::FlatNode>(buffer.data());
  auto obj2 = convertFlatNodeToWorkloadMetadata(node);
  EXPECT_EQ(obj2.baggage(), obj.baggage());
}

TEST(WorkloadMetadataObjectTest, FromBaggage) {
  {
    auto obj = WorkloadMetadataObject::fromBaggage(
        absl::StrCat("k8s.deployment.name=foo,k8s.cluster.name=my-cluster,k8s."
                     "namespace.name=default,",
                     "service.name=foo-service,", "service.version=v1alpha3"));
    EXPECT_EQ(obj.canonical_name_, "foo-service");
    EXPECT_EQ(obj.canonical_revision_, "v1alpha3");
    EXPECT_EQ(obj.workload_type_, WorkloadType::Deployment);
    EXPECT_EQ(obj.workload_name_, "foo");
    EXPECT_EQ(obj.namespace_name_, "default");
    EXPECT_EQ(obj.cluster_name_, "my-cluster");
    checkFlatNodeConversion(obj);
  }

  {
    auto obj = WorkloadMetadataObject::fromBaggage(
        absl::StrCat("k8s.pod.name=foo-pod-435,k8s.cluster.name=my-cluster,k8s."
                     "namespace.name=test,"
                     "service.name=foo-service,service.version=v1beta2"));

    EXPECT_EQ(obj.canonical_name_, "foo-service");
    EXPECT_EQ(obj.canonical_revision_, "v1beta2");
    EXPECT_EQ(obj.workload_type_, WorkloadType::Pod);
    EXPECT_EQ(obj.workload_name_, "foo-pod-435");
    EXPECT_EQ(obj.instance_name_, "foo-pod-435");
    EXPECT_EQ(obj.namespace_name_, "test");
    EXPECT_EQ(obj.cluster_name_, "my-cluster");
    checkFlatNodeConversion(obj);
  }

  {
    auto obj = WorkloadMetadataObject::fromBaggage(
        absl::StrCat("k8s.job.name=foo-job-435,k8s.cluster.name=my-cluster,k8s."
                     "namespace.name=test,",
                     "service.name=foo-service,", "service.version=v1beta4"));

    EXPECT_EQ(obj.canonical_name_, "foo-service");
    EXPECT_EQ(obj.canonical_revision_, "v1beta4");
    EXPECT_EQ(obj.workload_type_, WorkloadType::Job);
    EXPECT_EQ(obj.workload_name_, "foo-job-435");
    EXPECT_EQ(obj.instance_name_, "foo-job-435");
    EXPECT_EQ(obj.namespace_name_, "test");
    EXPECT_EQ(obj.cluster_name_, "my-cluster");
    checkFlatNodeConversion(obj);
  }

  {
    auto obj = WorkloadMetadataObject::fromBaggage(
        absl::StrCat("k8s.cronjob.name=foo-cronjob,k8s.cluster.name=my-cluster,"
                     "k8s.namespace.name=test,",
                     "service.name=foo-service,", "service.version=v1beta4"));

    EXPECT_EQ(obj.canonical_name_, "foo-service");
    EXPECT_EQ(obj.canonical_revision_, "v1beta4");
    EXPECT_EQ(obj.workload_type_, WorkloadType::CronJob);
    EXPECT_EQ(obj.workload_name_, "foo-cronjob");
    EXPECT_EQ(obj.namespace_name_, "test");
    EXPECT_EQ(obj.cluster_name_, "my-cluster");
    EXPECT_EQ(obj.app_name_, "");
    EXPECT_EQ(obj.app_version_, "");
    checkFlatNodeConversion(obj);
  }

  {
    auto obj = WorkloadMetadataObject::fromBaggage(absl::StrCat(
        "k8s.deployment.name=foo,k8s.namespace.name=default,", "service.name=foo-service,",
        "service.version=v1alpha3,app.name=foo-app,app.version=v1"));

    EXPECT_EQ(obj.canonical_name_, "foo-service");
    EXPECT_EQ(obj.canonical_revision_, "v1alpha3");
    EXPECT_EQ(obj.workload_type_, WorkloadType::Deployment);
    EXPECT_EQ(obj.workload_name_, "foo");
    EXPECT_EQ(obj.namespace_name_, "default");
    EXPECT_EQ(obj.cluster_name_, "");
    EXPECT_EQ(obj.app_name_, "foo-app");
    EXPECT_EQ(obj.app_version_, "v1");
    checkFlatNodeConversion(obj);
  }
}

TEST(WorkloadMetadataObjectTest, ConvertFromFlatNode) {
  flatbuffers::FlatBufferBuilder fbb;
  Wasm::Common::FlatNodeBuilder builder(fbb);
  auto data = builder.Finish();
  fbb.Finish(data);
  auto buffer = fbb.Release();
  const auto& node = *flatbuffers::GetRoot<Wasm::Common::FlatNode>(buffer.data());
  auto obj = convertFlatNodeToWorkloadMetadata(node);
  EXPECT_EQ(obj.baggage(), "k8s.pod.name=");
}

TEST(WorkloadMetadataObjectTest, ConvertFromEndpointMetadata) {
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata(""));
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata("a;b"));
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata("a;;;b"));
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata("a;b;c;d"));
  auto obj = convertEndpointMetadata("foo-pod;default;foo-service;v1;my-cluster");
  ASSERT_TRUE(obj.has_value());
  EXPECT_EQ(obj->baggage(), "k8s.pod.name=foo-pod,k8s.cluster.name=my-cluster,k8s.namespace."
                            "name=default,service.name=foo-service,service.version=v1");
}

} // namespace Common
} // namespace Istio
