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

#include "envoy/registry/registry.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Istio {
namespace Common {

using Envoy::Protobuf::util::MessageDifferencer;
using ::testing::NiceMock;

TEST(WorkloadMetadataObjectTest, Baggage) {
  WorkloadMetadataObject deploy("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                                "v1alpha3", "", "", WorkloadType::Deployment, "");

  WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                             "v1alpha3", "", "", WorkloadType::Pod, "");

  WorkloadMetadataObject cronjob("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                                 "v1alpha3", "foo-app", "v1", WorkloadType::CronJob, "");

  WorkloadMetadataObject job("pod-foo-1234", "my-cluster", "default", "foo", "foo-service",
                             "v1alpha3", "", "", WorkloadType::Job, "");

  EXPECT_EQ(deploy.serializeAsString(),
            absl::StrCat("type=deployment,workload=foo,name=pod-foo-1234,cluster=my-cluster,",
                         "namespace=default,service=foo-service,revision=v1alpha3"));

  EXPECT_EQ(pod.serializeAsString(),
            absl::StrCat("type=pod,workload=foo,name=pod-foo-1234,cluster=my-cluster,",
                         "namespace=default,service=foo-service,revision=v1alpha3"));

  EXPECT_EQ(cronjob.serializeAsString(),
            absl::StrCat("type=cronjob,workload=foo,name=pod-foo-1234,cluster=my-cluster,",
                         "namespace=default,service=foo-service,revision=v1alpha3,",
                         "app=foo-app,version=v1"));

  EXPECT_EQ(job.serializeAsString(),
            absl::StrCat("type=job,workload=foo,name=pod-foo-1234,cluster=my-cluster,",
                         "namespace=default,service=foo-service,revision=v1alpha3"));
}

void checkStructConversion(const Envoy::StreamInfo::FilterState::Object& data) {
  const auto& obj = dynamic_cast<const WorkloadMetadataObject&>(data);
  auto pb = convertWorkloadMetadataToStruct(obj);
  auto obj2 = convertStructToWorkloadMetadata(pb);
  EXPECT_EQ(obj2->serializeAsString(), obj.serializeAsString());
  MessageDifferencer::Equals(*(obj2->serializeAsProto()), *(obj.serializeAsProto()));
  EXPECT_EQ(obj2->hash(), obj.hash());
}

TEST(WorkloadMetadataObjectTest, Conversion) {
  {
    const auto r = convertBaggageToWorkloadMetadata(
        "type=deployment,workload=foo,cluster=my-cluster,"
        "namespace=default,service=foo-service,revision=v1alpha3,app=foo-app,version=latest");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("service")), "foo-service");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("revision")), "v1alpha3");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("type")), DeploymentSuffix);
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("workload")), "foo");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("name")), "");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("namespace")), "default");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("cluster")), "my-cluster");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("app")), "foo-app");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("version")), "latest");
    checkStructConversion(*r);
  }

  {
    const auto r =
        convertBaggageToWorkloadMetadata("type=pod,name=foo-pod-435,cluster=my-cluster,namespace="
                                         "test,service=foo-service,revision=v1beta2");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("service")), "foo-service");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("revision")), "v1beta2");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("type")), PodSuffix);
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("workload")), "");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("name")), "foo-pod-435");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("namespace")), "test");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("cluster")), "my-cluster");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("app")), "");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("version")), "");
    checkStructConversion(*r);
  }

  {
    const auto r =
        convertBaggageToWorkloadMetadata("type=job,name=foo-job-435,cluster=my-cluster,namespace="
                                         "test,service=foo-service,revision=v1beta4");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("service")), "foo-service");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("revision")), "v1beta4");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("type")), JobSuffix);
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("workload")), "");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("name")), "foo-job-435");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("namespace")), "test");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("cluster")), "my-cluster");
    checkStructConversion(*r);
  }

  {
    const auto r =
        convertBaggageToWorkloadMetadata("type=cronjob,workload=foo-cronjob,cluster=my-cluster,"
                                         "namespace=test,service=foo-service,revision=v1beta4");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("service")), "foo-service");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("revision")), "v1beta4");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("type")), CronJobSuffix);
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("workload")), "foo-cronjob");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("name")), "");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("namespace")), "test");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("cluster")), "my-cluster");
    checkStructConversion(*r);
  }

  {
    const auto r = convertBaggageToWorkloadMetadata(
        "type=deployment,workload=foo,namespace=default,service=foo-service,revision=v1alpha3");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("service")), "foo-service");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("revision")), "v1alpha3");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("type")), DeploymentSuffix);
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("workload")), "foo");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("namespace")), "default");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("cluster")), "");
    checkStructConversion(*r);
  }

  {
    const auto r = convertBaggageToWorkloadMetadata("namespace=default");
    EXPECT_EQ(absl::get<absl::string_view>(r->getField("namespace")), "default");
    checkStructConversion(*r);
  }
}

TEST(WorkloadMetadataObjectTest, ConvertFromEmpty) {
  google::protobuf::Struct node;
  auto obj = convertStructToWorkloadMetadata(node);
  EXPECT_EQ(obj->serializeAsString(), "");
  checkStructConversion(*obj);
}

TEST(WorkloadMetadataObjectTest, ConvertFromEndpointMetadata) {
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata(""));
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata("a;b"));
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata("a;;;b"));
  EXPECT_EQ(absl::nullopt, convertEndpointMetadata("a;b;c;d"));
  auto obj = convertEndpointMetadata("foo-pod;default;foo-service;v1;my-cluster");
  ASSERT_TRUE(obj.has_value());
  EXPECT_EQ(obj->serializeAsString(), "workload=foo-pod,cluster=my-cluster,"
                                      "namespace=default,service=foo-service,revision=v1");
}

} // namespace Common
} // namespace Istio
