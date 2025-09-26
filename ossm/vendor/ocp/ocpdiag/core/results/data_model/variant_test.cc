// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/variant.h"

#include "gtest/gtest.h"

namespace ocpdiag::results {

TEST(TestVariant, TextLiteralStoresAsString) {
  constexpr char kLiteral[] = "Test string";
  Variant variant(kLiteral);
  std::string* output = std::get_if<std::string>(&variant);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(*output, kLiteral);
  EXPECT_EQ(std::get_if<bool>(&variant), nullptr);
  EXPECT_EQ(std::get_if<double>(&variant), nullptr);
}

TEST(TestVariant, StringStoresAsString) {
  std::string str = "Test string";
  Variant variant(str);
  std::string* output = std::get_if<std::string>(&variant);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(*output, str);
  EXPECT_EQ(std::get_if<bool>(&variant), nullptr);
  EXPECT_EQ(std::get_if<double>(&variant), nullptr);
}

TEST(TestVariant, BoolStoresAsBool) {
  Variant variant(true);
  bool* output = std::get_if<bool>(&variant);
  ASSERT_NE(output, nullptr);
  EXPECT_TRUE(*output);
  EXPECT_EQ(std::get_if<std::string>(&variant), nullptr);
  EXPECT_EQ(std::get_if<double>(&variant), nullptr);
}

TEST(TestVariant, DoubleStoresAsDouble) {
  Variant variant(3.4);
  double* output = std::get_if<double>(&variant);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(*output, 3.4);
  EXPECT_EQ(std::get_if<std::string>(&variant), nullptr);
  EXPECT_EQ(std::get_if<bool>(&variant), nullptr);
}

}  // namespace ocpdiag::results
