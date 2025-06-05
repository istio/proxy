// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_weak_ptr.h"

#include <utility>

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

}  // namespace
}  // namespace quiche
