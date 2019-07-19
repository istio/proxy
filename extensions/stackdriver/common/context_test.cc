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

#include "extensions/stackdriver/common/context.h"
#include "extensions/stackdriver/common/constants.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/stubs/status.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Common {

using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace stackdriver::common;

// Test all possible metadata field.
TEST(ContextTest, ExtractNodeMetadata) {
  std::string node_metadata = R"###(
fields {
  key: "name"
  value {
    string_value: "test_pod"
  }
}
fields {
  key: "namespace"
  value {
    string_value: "test_namespace"
  }
}
fields {
  key: "owner"
  value {
    string_value: "test_owner"
  }
}
fields {
  key: "platform_metadata"
  value {
    struct_value {
      fields {
        key: "gcp_cluster_location"
        value {
          string_value: "test_location"
        }
      }
      fields {
        key: "gcp_cluster_name"
        value {
          string_value: "test_cluster"
        }
      }
      fields {
        key: "gcp_project"
        value {
          string_value: "test_project"
        }
      }
    }
  }
}
fields {
  key: "ports_to_containers"
  value {
    struct_value {
      fields {
        key: "80"
        value {
          string_value: "test_container"
        }
      }
    }
  }
}
fields {
  key: "workload_name"
  value {
    string_value: "test_workload"
  }
}
)###";
  google::protobuf::Struct metadata_struct;
  TextFormat::ParseFromString(node_metadata, &metadata_struct);
  NodeInfo node_info;
  Status status = ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "test_pod");
  EXPECT_EQ(node_info.namespace_(), "test_namespace");
  EXPECT_EQ(node_info.owner(), "test_owner");
  EXPECT_EQ(node_info.workload_name(), "test_workload");
  EXPECT_EQ(node_info.platform_metadata().gcp_project(), "test_project");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "test_cluster");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(),
            "test_location");
  EXPECT_EQ(node_info.ports_to_containers().size(), 1);
  EXPECT_EQ(node_info.ports_to_containers().at("80"), "test_container");
}

// Test empty node metadata.
TEST(ContextTest, ExtractNodeMetadataNoMetadataField) {
  google::protobuf::Struct metadata_struct;
  NodeInfo node_info;

  Status status = ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "");
  EXPECT_EQ(node_info.namespace_(), "");
  EXPECT_EQ(node_info.owner(), "");
  EXPECT_EQ(node_info.workload_name(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_project(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(), "");
  EXPECT_EQ(node_info.ports_to_containers().size(), 0);
}

// Test missing metadata.
TEST(ContextTest, ExtractNodeMetadataMissingMetadata) {
  std::string node_metadata = R"###(
fields {
  key: "name"
  value {
    string_value: "test_pod"
  }
}
fields {
  key: "namespace"
  value {
    string_value: "test_namespace"
  }
}
)###";
  ;
  google::protobuf::Struct metadata_struct;
  TextFormat::ParseFromString(node_metadata, &metadata_struct);
  NodeInfo node_info;
  Status status = ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "test_pod");
  EXPECT_EQ(node_info.namespace_(), "test_namespace");
  EXPECT_EQ(node_info.owner(), "");
  EXPECT_EQ(node_info.workload_name(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_project(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(), "");
  EXPECT_EQ(node_info.ports_to_containers().size(), 0);
}

// Test wrong type of GCP metadata.
TEST(ContextTest, ExtractNodeMetadataWrongGCPMetadata) {
  std::string node_metadata = R"###(
fields {
  key: "platform_metadata"
  value {
    string_value: "some string"
  }
}
)###";
  google::protobuf::Struct metadata_struct;
  TextFormat::ParseFromString(node_metadata, &metadata_struct);
  NodeInfo node_info;
  Status status = ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_NE(status, Status::OK);
  EXPECT_EQ(node_info.platform_metadata().gcp_project(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "");
  EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(), "");
  EXPECT_EQ(node_info.ports_to_containers().size(), 0);
}

// Test unknown field.
TEST(ContextTest, ExtractNodeMetadataUnknownField) {
  std::string node_metadata = R"###(
fields {
  key: "some metadat"
  value {
    string_value: "some string"
  }
}
)###";
  google::protobuf::Struct metadata_struct;
  TextFormat::ParseFromString(node_metadata, &metadata_struct);
  NodeInfo node_info;
  Status status = ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
}

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions
