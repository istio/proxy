// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/platform/api/quiche_url_utils.h"

#include <optional>
#include <set>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace {

void ValidateExpansion(
    const std::string& uri_template,
    const absl::flat_hash_map<std::string, std::string>& parameters,
    const std::string& expected_expansion,
    const absl::flat_hash_set<std::string>& expected_vars_found) {
  absl::flat_hash_set<std::string> vars_found;
  std::string target;
  ASSERT_TRUE(
      ExpandURITemplate(uri_template, parameters, &target, &vars_found));
  EXPECT_EQ(expected_expansion, target);
  EXPECT_EQ(vars_found, expected_vars_found);
}

TEST(QuicheUrlUtilsTest, Basic) {
  ValidateExpansion("/{foo}/{bar}/", {{"foo", "123"}, {"bar", "456"}},
                    "/123/456/", {"foo", "bar"});
}

TEST(QuicheUrlUtilsTest, ExtraParameter) {
  ValidateExpansion("/{foo}/{bar}/{baz}/", {{"foo", "123"}, {"bar", "456"}},
                    "/123/456//", {"foo", "bar"});
}

TEST(QuicheUrlUtilsTest, MissingParameter) {
  ValidateExpansion("/{foo}/{baz}/", {{"foo", "123"}, {"bar", "456"}}, "/123//",
                    {"foo"});
}

TEST(QuicheUrlUtilsTest, RepeatedParameter) {
  ValidateExpansion("/{foo}/{bar}/{foo}/", {{"foo", "123"}, {"bar", "456"}},
                    "/123/456/123/", {"foo", "bar"});
}

TEST(QuicheUrlUtilsTest, URLEncoding) {
  ValidateExpansion("/{foo}/{bar}/", {{"foo", "123"}, {"bar", ":"}},
                    "/123/%3A/", {"foo", "bar"});
}

void ValidateUrlDecode(const std::string& input,
                       const std::optional<std::string>& expected_output) {
  std::optional<std::string> decode_result = AsciiUrlDecode(input);
  if (!expected_output.has_value()) {
    EXPECT_FALSE(decode_result.has_value());
    return;
  }
  ASSERT_TRUE(decode_result.has_value());
  EXPECT_EQ(decode_result.value(), expected_output);
}

TEST(QuicheUrlUtilsTest, DecodeNoChange) {
  ValidateUrlDecode("foobar", "foobar");
}

TEST(QuicheUrlUtilsTest, DecodeReplace) {
  ValidateUrlDecode("%7Bfoobar%7D", "{foobar}");
}

TEST(QuicheUrlUtilsTest, DecodeFail) { ValidateUrlDecode("%FF", std::nullopt); }

}  // namespace
}  // namespace quiche
