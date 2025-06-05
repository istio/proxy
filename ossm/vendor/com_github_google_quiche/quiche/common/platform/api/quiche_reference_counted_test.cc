// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/platform/api/quiche_reference_counted.h"

#include <utility>

#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace test {
namespace {

class Base : public QuicheReferenceCounted {
 public:
  explicit Base(bool* destroyed) : destroyed_(destroyed) {
    *destroyed_ = false;
  }

 protected:
  ~Base() override { *destroyed_ = true; }

 private:
  bool* destroyed_;
};

class Derived : public Base {
 public:
  explicit Derived(bool* destroyed) : Base(destroyed) {}

 private:
  ~Derived() override {}
};

class QuicheReferenceCountedTest : public QuicheTest {};

TEST_F(QuicheReferenceCountedTest, DefaultConstructor) {
  QuicheReferenceCountedPointer<Base> a;
  EXPECT_EQ(nullptr, a);
  EXPECT_EQ(nullptr, a.get());
  EXPECT_FALSE(a);
}

TEST_F(QuicheReferenceCountedTest, ConstructFromRawPointer) {
  bool destroyed = false;
  {
    QuicheReferenceCountedPointer<Base> a(new Base(&destroyed));
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, RawPointerAssignment) {
  bool destroyed = false;
  {
    QuicheReferenceCountedPointer<Base> a;
    Base* rct = new Base(&destroyed);
    a = rct;
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerCopy) {
  bool destroyed = false;
  {
    QuicheReferenceCountedPointer<Base> a(new Base(&destroyed));
    {
      QuicheReferenceCountedPointer<Base> b(a);
      EXPECT_EQ(a, b);
      EXPECT_FALSE(destroyed);
    }
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerCopyAssignment) {
  bool destroyed = false;
  {
    QuicheReferenceCountedPointer<Base> a(new Base(&destroyed));
    {
      QuicheReferenceCountedPointer<Base> b = a;
      EXPECT_EQ(a, b);
      EXPECT_FALSE(destroyed);
    }
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerCopyFromOtherType) {
  bool destroyed = false;
  {
    QuicheReferenceCountedPointer<Derived> a(new Derived(&destroyed));
    {
      QuicheReferenceCountedPointer<Base> b(a);
      EXPECT_EQ(a.get(), b.get());
      EXPECT_FALSE(destroyed);
    }
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerCopyAssignmentFromOtherType) {
  bool destroyed = false;
  {
    QuicheReferenceCountedPointer<Derived> a(new Derived(&destroyed));
    {
      QuicheReferenceCountedPointer<Base> b = a;
      EXPECT_EQ(a.get(), b.get());
      EXPECT_FALSE(destroyed);
    }
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerMove) {
  bool destroyed = false;
  QuicheReferenceCountedPointer<Base> a(new Derived(&destroyed));
  EXPECT_FALSE(destroyed);
  QuicheReferenceCountedPointer<Base> b(std::move(a));
  EXPECT_FALSE(destroyed);
  EXPECT_NE(nullptr, b);
  EXPECT_EQ(nullptr, a);  // NOLINT

  b = nullptr;
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerMoveAssignment) {
  bool destroyed = false;
  QuicheReferenceCountedPointer<Base> a(new Derived(&destroyed));
  EXPECT_FALSE(destroyed);
  QuicheReferenceCountedPointer<Base> b = std::move(a);
  EXPECT_FALSE(destroyed);
  EXPECT_NE(nullptr, b);
  EXPECT_EQ(nullptr, a);  // NOLINT

  b = nullptr;
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerMoveFromOtherType) {
  bool destroyed = false;
  QuicheReferenceCountedPointer<Derived> a(new Derived(&destroyed));
  EXPECT_FALSE(destroyed);
  QuicheReferenceCountedPointer<Base> b(std::move(a));
  EXPECT_FALSE(destroyed);
  EXPECT_NE(nullptr, b);
  EXPECT_EQ(nullptr, a);  // NOLINT

  b = nullptr;
  EXPECT_TRUE(destroyed);
}

TEST_F(QuicheReferenceCountedTest, PointerMoveAssignmentFromOtherType) {
  bool destroyed = false;
  QuicheReferenceCountedPointer<Derived> a(new Derived(&destroyed));
  EXPECT_FALSE(destroyed);
  QuicheReferenceCountedPointer<Base> b = std::move(a);
  EXPECT_FALSE(destroyed);
  EXPECT_NE(nullptr, b);
  EXPECT_EQ(nullptr, a);  // NOLINT

  b = nullptr;
  EXPECT_TRUE(destroyed);
}

}  // namespace
}  // namespace test
}  // namespace quiche
