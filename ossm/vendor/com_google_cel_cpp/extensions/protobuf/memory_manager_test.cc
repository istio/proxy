// Copyright 2023 Google LLC
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

#include "extensions/protobuf/memory_manager.h"

#include "common/memory.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

TEST(ProtoMemoryManager, MemoryManagement) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManager(&arena);
  EXPECT_EQ(memory_manager.memory_management(), MemoryManagement::kPooling);
}

TEST(ProtoMemoryManager, Arena) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManager(&arena);
  EXPECT_THAT(ProtoMemoryManagerArena(memory_manager), NotNull());
}

TEST(ProtoMemoryManagerRef, MemoryManagement) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  EXPECT_EQ(memory_manager.memory_management(), MemoryManagement::kPooling);
  memory_manager = ProtoMemoryManagerRef(nullptr);
  EXPECT_EQ(memory_manager.memory_management(),
            MemoryManagement::kReferenceCounting);
}

TEST(ProtoMemoryManagerRef, Arena) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  EXPECT_THAT(ProtoMemoryManagerArena(memory_manager), Eq(&arena));
  memory_manager = ProtoMemoryManagerRef(nullptr);
  EXPECT_THAT(ProtoMemoryManagerArena(memory_manager), IsNull());
}

}  // namespace
}  // namespace cel::extensions
