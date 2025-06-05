// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/trivial_legacy_type_info.h"

#include "eval/public/message_wrapper.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

TEST(TrivialTypeInfo, GetTypename) {
  TrivialTypeInfo info;
  MessageWrapper wrapper;

  EXPECT_EQ(info.GetTypename(wrapper), "opaque");
  EXPECT_EQ(TrivialTypeInfo::GetInstance()->GetTypename(wrapper), "opaque");
}

TEST(TrivialTypeInfo, DebugString) {
  TrivialTypeInfo info;
  MessageWrapper wrapper;

  EXPECT_EQ(info.DebugString(wrapper), "opaque");
  EXPECT_EQ(TrivialTypeInfo::GetInstance()->DebugString(wrapper), "opaque");
}

TEST(TrivialTypeInfo, GetAccessApis) {
  TrivialTypeInfo info;
  MessageWrapper wrapper;

  EXPECT_EQ(info.GetAccessApis(wrapper), nullptr);
  EXPECT_EQ(TrivialTypeInfo::GetInstance()->GetAccessApis(wrapper), nullptr);
}

TEST(TrivialTypeInfo, GetMutationApis) {
  TrivialTypeInfo info;
  MessageWrapper wrapper;

  EXPECT_EQ(info.GetMutationApis(wrapper), nullptr);
  EXPECT_EQ(TrivialTypeInfo::GetInstance()->GetMutationApis(wrapper), nullptr);
}

TEST(TrivialTypeInfo, FindFieldByName) {
  TrivialTypeInfo info;
  MessageWrapper wrapper;

  EXPECT_EQ(info.FindFieldByName("foo"), absl::nullopt);
  EXPECT_EQ(TrivialTypeInfo::GetInstance()->FindFieldByName("foo"),
            absl::nullopt);
}

}  // namespace
}  // namespace google::api::expr::runtime
