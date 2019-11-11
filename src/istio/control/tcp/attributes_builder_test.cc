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

#include "src/istio/control/tcp/attributes_builder.h"

#include "google/protobuf/stubs/status.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "include/istio/utils/attribute_names.h"
#include "include/istio/utils/attributes_builder.h"
#include "src/istio/control/tcp/mock_check_data.h"
#include "src/istio/control/tcp/mock_report_data.h"
#include "src/istio/utils/utils.h"

using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

namespace istio {
namespace control {
namespace tcp {
namespace {

const char kCheckAttributes[] = R"(
attributes {
  key: "context.protocol"
  value {
    string_value: "tcp"
  }
}
attributes {
  key: "context.time"
  value {
    timestamp_value {
    }
  }
}
attributes {
  key: "source.ip"
  value {
    bytes_value: "1.2.3.4"
  }
}
attributes {
  key: "origin.ip"
  value {
    bytes_value: "1.2.3.4"
  }
}
attributes {
  key: "connection.mtls"
  value {
    bool_value: true
  }
}
attributes {
  key: "connection.requested_server_name"
  value {
    string_value: "www.google.com"
  }
}
attributes {
  key: "source.namespace"
  value {
    string_value: "ns_ns"
  }
}
attributes {
  key: "source.principal"
  value {
    string_value: "cluster.local/sa/test_user/ns/ns_ns/"
  }
}
attributes {
  key: "source.user"
  value {
    string_value: "cluster.local/sa/test_user/ns/ns_ns/"
  }
}
attributes {
  key: "destination.principal"
  value {
    string_value: "destination_user"
  }
}
attributes {
  key: "connection.id"
  value {
    string_value: "1234-5"
  }
}
)";

const char kFirstReportAttributes[] = R"(
attributes {
  key: "connection.event"
  value {
    string_value: "open"
  }
}
attributes {
  key: "connection.received.bytes"
  value {
    int64_value: 0
  }
}
attributes {
  key: "connection.received.bytes_total"
  value {
    int64_value: 0
  }
}
attributes {
  key: "connection.sent.bytes"
  value {
    int64_value: 0
  }
}
attributes {
  key: "connection.sent.bytes_total"
  value {
    int64_value: 0
  }
}
attributes {
  key: "context.time"
  value {
    timestamp_value {
    }
  }
}
attributes {
  key: "destination.ip"
  value {
    bytes_value: "1.2.3.4"
  }
}
attributes {
  key: "destination.port"
  value {
    int64_value: 8080
  }
}
attributes {
  key: "destination.uid"
  value {
    string_value: "pod1.ns2"
  }
}
attributes {
  key: "foo.bar.com"
  value {
    string_map_value {
      entries {
        key: "str"
        value: "abc"
      }
      entries {
        key: "list"
        value: "a,b,c"
      }
    }
  }
}
)";

const char kReportAttributes[] = R"(
attributes {
  key: "connection.event"
  value {
    string_value: "close"
  }
}
attributes {
  key: "check.error_code"
  value {
    int64_value: 3
  }
}
attributes {
  key: "check.error_message"
  value {
    string_value: "INVALID_ARGUMENT:Invalid argument"
  }
}
attributes {
  key: "connection.duration"
  value {
    duration_value {
      nanos: 4
    }
  }
}
attributes {
  key: "connection.received.bytes"
  value {
    int64_value: 144
  }
}
attributes {
  key: "connection.received.bytes_total"
  value {
    int64_value: 345
  }
}
attributes {
  key: "connection.sent.bytes"
  value {
    int64_value: 274
  }
}
attributes {
  key: "connection.sent.bytes_total"
  value {
    int64_value: 678
  }
}
attributes {
  key: "context.time"
  value {
    timestamp_value {
    }
  }
}
attributes {
  key: "destination.ip"
  value {
    bytes_value: "1.2.3.4"
  }
}
attributes {
  key: "destination.port"
  value {
    int64_value: 8080
  }
}
attributes {
  key: "destination.uid"
  value {
    string_value: "pod1.ns2"
  }
}
attributes {
  key: "foo.bar.com"
  value {
    string_map_value {
      entries {
        key: "str"
        value: "abc"
      }
      entries {
        key: "list"
        value: "a,b,c"
      }
    }
  }
}
)";

const char kDeltaOneReportAttributes[] = R"(
attributes {
  key: "connection.event"
  value {
    string_value: "continue"
  }
}
attributes {
  key: "connection.received.bytes"
  value {
    int64_value: 100
  }
}
attributes {
  key: "connection.sent.bytes"
  value {
    int64_value: 200
  }
}
attributes {
  key: "connection.received.bytes_total"
  value {
    int64_value: 100
  }
}
attributes {
  key: "connection.sent.bytes_total"
  value {
    int64_value: 200
  }
}
attributes {
  key: "context.time"
  value {
    timestamp_value {
    }
  }
}
attributes {
  key: "destination.ip"
  value {
    bytes_value: "1.2.3.4"
  }
}
attributes {
  key: "destination.port"
  value {
    int64_value: 8080
  }
}
attributes {
  key: "destination.uid"
  value {
    string_value: "pod1.ns2"
  }
}
attributes {
  key: "foo.bar.com"
  value {
    string_map_value {
      entries {
        key: "str"
        value: "abc"
      }
      entries {
        key: "list"
        value: "a,b,c"
      }
    }
  }
}
)";

const char kDeltaTwoReportAttributes[] = R"(
attributes {
  key: "connection.event"
  value {
    string_value: "continue"
  }
}
attributes {
  key: "connection.received.bytes"
  value {
    int64_value: 101
  }
}
attributes {
  key: "connection.sent.bytes"
  value {
    int64_value: 204
  }
}
attributes {
  key: "connection.received.bytes_total"
  value {
    int64_value: 201
  }
}
attributes {
  key: "connection.sent.bytes_total"
  value {
    int64_value: 404
  }
}
attributes {
  key: "context.time"
  value {
    timestamp_value {
    }
  }
}
attributes {
  key: "destination.ip"
  value {
    bytes_value: "1.2.3.4"
  }
}
attributes {
  key: "destination.port"
  value {
    int64_value: 8080
  }
}
attributes {
  key: "destination.uid"
  value {
    string_value: "pod1.ns2"
  }
}
attributes {
  key: "foo.bar.com"
  value {
    string_map_value {
      entries {
        key: "str"
        value: "abc"
      }
      entries {
        key: "list"
        value: "a,b,c"
      }
    }
  }
}
)";

void ClearContextTime(istio::mixer::v1::Attributes *attributes) {
  // Override timestamp with -
  utils::AttributesBuilder builder(attributes);
  std::chrono::time_point<std::chrono::system_clock> time0;
  builder.AddTimestamp(utils::AttributeName::kContextTime, time0);
}

TEST(AttributesBuilderTest, TestCheckAttributes) {
  ::testing::NiceMock<MockCheckData> mock_data;
  EXPECT_CALL(mock_data, GetSourceIpPort(_, _))
      .WillOnce(Invoke([](std::string *ip, int *port) -> bool {
        *ip = "1.2.3.4";
        *port = 8080;
        return true;
      }));
  EXPECT_CALL(mock_data, IsMutualTLS()).WillOnce(Invoke([]() -> bool {
    return true;
  }));
  EXPECT_CALL(mock_data, GetPrincipal(_, _))
      .WillRepeatedly(Invoke([](bool peer, std::string *user) -> bool {
        if (peer) {
          *user = "cluster.local/sa/test_user/ns/ns_ns/";
        } else {
          *user = "destination_user";
        }
        return true;
      }));
  EXPECT_CALL(mock_data, GetConnectionId()).WillOnce(Return("1234-5"));
  EXPECT_CALL(mock_data, GetRequestedServerName(_))
      .WillOnce(Invoke([](std::string *name) -> bool {
        *name = "www.google.com";
        return true;
      }));
  istio::mixer::v1::Attributes attributes;
  AttributesBuilder builder(&attributes);
  builder.ExtractCheckAttributes(&mock_data);

  ClearContextTime(&attributes);

  std::string out_str;
  TextFormat::PrintToString(attributes, &out_str);
  GOOGLE_LOG(INFO) << "===" << out_str << "===";

  ::istio::mixer::v1::Attributes expected_attributes;
  ASSERT_TRUE(
      TextFormat::ParseFromString(kCheckAttributes, &expected_attributes));
  EXPECT_TRUE(MessageDifferencer::Equals(attributes, expected_attributes));
}

TEST(AttributesBuilderTest, TestReportAttributes) {
  ::testing::NiceMock<MockReportData> mock_data;

  ::google::protobuf::Map<std::string, ::google::protobuf::Struct>
      filter_metadata;
  ::google::protobuf::Struct struct_obj;
  ::google::protobuf::Value strval, numval, boolval, listval;
  strval.set_string_value("abc");
  (*struct_obj.mutable_fields())["str"] = strval;
  numval.set_number_value(12.3);
  (*struct_obj.mutable_fields())["num"] = numval;
  boolval.set_bool_value(true);
  (*struct_obj.mutable_fields())["bool"] = boolval;
  listval.mutable_list_value()->add_values()->set_string_value("a");
  listval.mutable_list_value()->add_values()->set_string_value("b");
  listval.mutable_list_value()->add_values()->set_string_value("c");
  (*struct_obj.mutable_fields())["list"] = listval;
  filter_metadata["foo.bar.com"] = struct_obj;
  filter_metadata["istio.mixer"] = struct_obj;  // to be ignored

  EXPECT_CALL(mock_data, GetDestinationIpPort(_, _))
      .Times(4)
      .WillRepeatedly(Invoke([](std::string *ip, int *port) -> bool {
        *ip = "1.2.3.4";
        *port = 8080;
        return true;
      }));
  EXPECT_CALL(mock_data, GetDestinationUID(_))
      .Times(4)
      .WillRepeatedly(Invoke([](std::string *uid) -> bool {
        *uid = "pod1.ns2";
        return true;
      }));
  EXPECT_CALL(mock_data, GetDynamicFilterState())
      .Times(4)
      .WillRepeatedly(ReturnRef(filter_metadata));
  EXPECT_CALL(mock_data, GetReportInfo(_))
      .Times(4)
      .WillOnce(Invoke([](ReportData::ReportInfo *info) {
        info->received_bytes = 0;
        info->send_bytes = 0;
        info->duration = std::chrono::nanoseconds(1);
      }))
      .WillOnce(Invoke([](ReportData::ReportInfo *info) {
        info->received_bytes = 100;
        info->send_bytes = 200;
        info->duration = std::chrono::nanoseconds(2);
      }))
      .WillOnce(Invoke([](ReportData::ReportInfo *info) {
        info->received_bytes = 201;
        info->send_bytes = 404;
        info->duration = std::chrono::nanoseconds(3);
      }))
      .WillOnce(Invoke([](ReportData::ReportInfo *info) {
        info->received_bytes = 345;
        info->send_bytes = 678;
        info->duration = std::chrono::nanoseconds(4);
      }));

  istio::mixer::v1::Attributes attributes;
  AttributesBuilder builder(&attributes);
  auto check_status = ::google::protobuf::util::Status(
      ::google::protobuf::util::error::INVALID_ARGUMENT, "Invalid argument");

  ReportData::ReportInfo last_report_info{0ULL, 0ULL,
                                          std::chrono::nanoseconds::zero()};
  // Verify first open report
  builder.ExtractReportAttributes(check_status, &mock_data,
                                  ReportData::ConnectionEvent::OPEN,
                                  &last_report_info);
  ClearContextTime(&attributes);

  std::string out_str;
  TextFormat::PrintToString(attributes, &out_str);
  GOOGLE_LOG(INFO) << "===" << out_str << "===";

  ::istio::mixer::v1::Attributes expected_open_attributes;
  ASSERT_TRUE(TextFormat::ParseFromString(kFirstReportAttributes,
                                          &expected_open_attributes));
  EXPECT_TRUE(MessageDifferencer::Equals(attributes, expected_open_attributes));
  EXPECT_EQ(0, last_report_info.received_bytes);
  EXPECT_EQ(0, last_report_info.send_bytes);

  // Verify delta one report
  builder.ExtractReportAttributes(check_status, &mock_data,
                                  ReportData::ConnectionEvent::CONTINUE,
                                  &last_report_info);
  ClearContextTime(&attributes);

  TextFormat::PrintToString(attributes, &out_str);
  GOOGLE_LOG(INFO) << "===" << out_str << "===";

  ::istio::mixer::v1::Attributes expected_delta_attributes;
  ASSERT_TRUE(TextFormat::ParseFromString(kDeltaOneReportAttributes,
                                          &expected_delta_attributes));
  EXPECT_TRUE(
      MessageDifferencer::Equals(attributes, expected_delta_attributes));
  EXPECT_EQ(100, last_report_info.received_bytes);
  EXPECT_EQ(200, last_report_info.send_bytes);

  // Verify delta two report
  builder.ExtractReportAttributes(check_status, &mock_data,
                                  ReportData::ConnectionEvent::CONTINUE,
                                  &last_report_info);
  ClearContextTime(&attributes);

  out_str.clear();
  TextFormat::PrintToString(attributes, &out_str);
  GOOGLE_LOG(INFO) << "===" << out_str << "===";

  expected_delta_attributes.Clear();
  ASSERT_TRUE(TextFormat::ParseFromString(kDeltaTwoReportAttributes,
                                          &expected_delta_attributes));
  EXPECT_TRUE(
      MessageDifferencer::Equals(attributes, expected_delta_attributes));
  EXPECT_EQ(201, last_report_info.received_bytes);
  EXPECT_EQ(404, last_report_info.send_bytes);

  // Verify final report
  builder.ExtractReportAttributes(check_status, &mock_data,
                                  ReportData::ConnectionEvent::CLOSE,
                                  &last_report_info);
  ClearContextTime(&attributes);

  out_str.clear();
  TextFormat::PrintToString(attributes, &out_str);
  GOOGLE_LOG(INFO) << "===" << out_str << "===";

  ::istio::mixer::v1::Attributes expected_final_attributes;
  ASSERT_TRUE(TextFormat::ParseFromString(kReportAttributes,
                                          &expected_final_attributes));
  EXPECT_TRUE(
      MessageDifferencer::Equals(attributes, expected_final_attributes));
}

}  // namespace
}  // namespace tcp
}  // namespace control
}  // namespace istio
