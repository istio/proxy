// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_weak_ptr.h"

#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace {

class TestClass {
 public:
  QuicheWeakPtr<TestClass> CreateWeakPtr() { return weak_factory_.Create(); }

 private:
  QuicheWeakPtrFactory<TestClass> weak_factory_{this};
};

TEST(QuicheWeakPtrTest, Empty) {
  QuicheWeakPtr<TestClass> ptr;
  EXPECT_FALSE(ptr.IsValid());
  EXPECT_EQ(ptr.GetIfAvailable(), nullptr);
}

TEST(QuicheWeakPtrTest, Valid) {
  TestClass object;
  QuicheWeakPtr<TestClass> ptr = object.CreateWeakPtr();
  EXPECT_TRUE(ptr.IsValid());
  EXPECT_EQ(ptr.GetIfAvailable(), &object);
}

TEST(QuicheWeakPtrTest, ValidCopy) {
  TestClass object;
  QuicheWeakPtr<TestClass> ptr = object.CreateWeakPtr();
  QuicheWeakPtr<TestClass> ptr_copy = ptr;
  EXPECT_TRUE(ptr.IsValid());
  EXPECT_TRUE(ptr_copy.IsValid());
  EXPECT_EQ(ptr.GetIfAvailable(), &object);
  EXPECT_EQ(ptr_copy.GetIfAvailable(), &object);
}

TEST(QuicheWeakPtrTest, EmptyAfterMove) {
  TestClass object;
  QuicheWeakPtr<TestClass> ptr = object.CreateWeakPtr();
  QuicheWeakPtr<TestClass> ptr_moved = std::move(ptr);
  EXPECT_FALSE(ptr.IsValid());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(ptr_moved.IsValid());
  EXPECT_EQ(ptr.GetIfAvailable(), nullptr);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(ptr_moved.GetIfAvailable(), &object);
}

TEST(QuicheWeakPtrTest, Expired) {
  QuicheWeakPtr<TestClass> ptr;
  {
    TestClass object;
    ptr = object.CreateWeakPtr();
    EXPECT_TRUE(ptr.IsValid());
  }
  EXPECT_FALSE(ptr.IsValid());
}

TEST(QuicheWeakPtrTest, Eq) {
  alignas(TestClass) char data[sizeof(TestClass)];

  // Two weak pointers to the same object are equal.
  TestClass* object1 = new (data) TestClass;
  QuicheWeakPtr<TestClass> ptr1 = object1->CreateWeakPtr();
  QuicheWeakPtr<TestClass> ptr2 = object1->CreateWeakPtr();
  EXPECT_EQ(ptr1, ptr2);

  // The equality continues to hold even if the original object got deleted.
  object1->~TestClass();
  EXPECT_FALSE(ptr1.IsValid());
  EXPECT_FALSE(ptr2.IsValid());
  EXPECT_EQ(ptr1, ptr2);

  // If a new object gets allocated in the exact same spot, the weak pointer to
  // the old object is not equal to the weak pointer to the new object.
  TestClass* object2 = new (data) TestClass;
  QuicheWeakPtr<TestClass> ptr3 = object2->CreateWeakPtr();
  EXPECT_NE(ptr1, ptr3);
  EXPECT_NE(ptr2, ptr3);
  EXPECT_EQ(ptr3.GetIfAvailable(), object1);

  // The null pointers are equal to themselves, but not to any object that is,
  // or was valid.
  QuicheWeakPtr<TestClass> ptr4;
  QuicheWeakPtr<TestClass> ptr5;
  EXPECT_EQ(ptr4, ptr5);
  EXPECT_NE(ptr4, ptr1);
  EXPECT_NE(ptr4, ptr3);

  object2->~TestClass();
}

TEST(QuicheWeakPtrTest, Hash) {
  TestClass object;
  QuicheWeakPtr<TestClass> ptr1 = object.CreateWeakPtr();
  QuicheWeakPtr<TestClass> ptr2 = object.CreateWeakPtr();
  EXPECT_EQ(absl::HashOf(ptr1), absl::HashOf(ptr2));

  absl::flat_hash_set<QuicheWeakPtr<TestClass>> set;
  EXPECT_EQ(set.size(), 0u);
  set.insert(ptr1);
  EXPECT_EQ(set.size(), 1u);
  set.insert(ptr2);
  EXPECT_EQ(set.size(), 1u);
  set.insert(TestClass().CreateWeakPtr());
  EXPECT_EQ(set.size(), 2u);
}

}  // namespace
}  // namespace quiche
