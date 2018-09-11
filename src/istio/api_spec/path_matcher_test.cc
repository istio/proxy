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

#include "src/istio/api_spec/path_matcher.h"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::ReturnRef;

namespace istio {
namespace api_spec {

namespace {

struct MethodInfo {};

class PathMatcherTest : public ::testing::Test {
 protected:
  PathMatcherTest() {}
  ~PathMatcherTest() {}

  MethodInfo* AddPath(std::string http_method, std::string http_template) {
    auto method = new MethodInfo();
    if (!builder_.Register(http_method, http_template, std::string(), method)) {
      delete method;
      return nullptr;
    }
    stored_methods_.emplace_back(method);
    return method;
  }

  MethodInfo* AddGetPath(std::string path) { return AddPath("GET", path); }

  void Build() { matcher_ = builder_.Build(); }

  MethodInfo* Lookup(std::string method, std::string path) {
    return matcher_->Lookup(method, path);
  }

 private:
  PathMatcherBuilder<MethodInfo*> builder_;
  PathMatcherPtr<MethodInfo*> matcher_;
  std::vector<std::unique_ptr<MethodInfo>> stored_methods_;
};

TEST_F(PathMatcherTest, WildCardMatchesRoot) {
  MethodInfo* data = AddGetPath("/**");
  Build();

  EXPECT_NE(nullptr, data);

  EXPECT_EQ(Lookup("GET", "/"), data);
  EXPECT_EQ(Lookup("GET", "/a"), data);
  EXPECT_EQ(Lookup("GET", "/a/"), data);
}

TEST_F(PathMatcherTest, WildCardMatches) {
  // '*' only matches one path segment, but '**' matches the remaining path.
  MethodInfo* a__ = AddGetPath("/a/**");
  MethodInfo* b_ = AddGetPath("/b/*");
  MethodInfo* c_d__ = AddGetPath("/c/*/d/**");
  MethodInfo* c_de = AddGetPath("/c/*/d/e");
  MethodInfo* cfde = AddGetPath("/c/f/d/e");
  Build();

  EXPECT_NE(nullptr, a__);
  EXPECT_NE(nullptr, b_);
  EXPECT_NE(nullptr, c_d__);
  EXPECT_NE(nullptr, c_de);
  EXPECT_NE(nullptr, cfde);

  EXPECT_EQ(Lookup("GET", "/a/b"), a__);
  EXPECT_EQ(Lookup("GET", "/a/b/c"), a__);
  EXPECT_EQ(Lookup("GET", "/b/c"), b_);

  EXPECT_EQ(Lookup("GET", "b/c/d"), nullptr);
  EXPECT_EQ(Lookup("GET", "/c/u/d/v"), c_d__);
  EXPECT_EQ(Lookup("GET", "/c/v/d/w/x"), c_d__);
  EXPECT_EQ(Lookup("GET", "/c/x/y/d/z"), nullptr);
  EXPECT_EQ(Lookup("GET", "/c//v/d/w/x"), nullptr);

  // Test that more specific match overrides wildcard "**"" match.
  EXPECT_EQ(Lookup("GET", "/c/x/d/e"), c_de);
  // Test that more specific match overrides wildcard "*"" match.
  EXPECT_EQ(Lookup("GET", "/c/f/d/e"), cfde);
}

TEST_F(PathMatcherTest, WildCardMethodMatches) {
  MethodInfo* a__ = AddPath("*", "/a/**");
  MethodInfo* b_ = AddPath("*", "/b/*");
  Build();

  EXPECT_NE(nullptr, a__);
  EXPECT_NE(nullptr, b_);

  std::vector<std::string> all_methods{"GET", "POST", "DELETE", "PATCH", "PUT"};
  for (const auto& method : all_methods) {
    EXPECT_EQ(Lookup(method, "/a/b"), a__);
    EXPECT_EQ(Lookup(method, "/a/b/c"), a__);
    EXPECT_EQ(Lookup(method, "/b/c"), b_);
  }
}

TEST_F(PathMatcherTest, InvalidTemplates) {
  EXPECT_EQ(nullptr, AddGetPath("/a{x=b/**}/{y=*}"));
  EXPECT_EQ(nullptr, AddGetPath("/a{x=b/**}/bb/{y=*}"));
  EXPECT_EQ(nullptr, AddGetPath("/a{x=b/**}/{y=**}"));
  EXPECT_EQ(nullptr, AddGetPath("/a{x=b/**}/bb/{y=**}"));

  EXPECT_EQ(nullptr, AddGetPath("/a/**/*"));
  EXPECT_EQ(nullptr, AddGetPath("/a/**/foo/*"));
  EXPECT_EQ(nullptr, AddGetPath("/a/**/**"));
  EXPECT_EQ(nullptr, AddGetPath("/a/**/foo/**"));
}

TEST_F(PathMatcherTest, CustomVerbMatches) {
  MethodInfo* some_const_verb = AddGetPath("/some/const:verb");
  MethodInfo* some__verb = AddGetPath("/some/*:verb");
  MethodInfo* some__foo_verb = AddGetPath("/some/*/foo:verb");
  MethodInfo* other__verb = AddGetPath("/other/**:verb");
  MethodInfo* other__const_verb = AddGetPath("/other/**/const:verb");
  Build();

  EXPECT_NE(nullptr, some_const_verb);
  EXPECT_NE(nullptr, some__verb);
  EXPECT_NE(nullptr, some__foo_verb);
  EXPECT_NE(nullptr, other__verb);
  EXPECT_NE(nullptr, other__const_verb);

  EXPECT_EQ(Lookup("GET", "/some/const:verb"), some_const_verb);
  EXPECT_EQ(Lookup("GET", "/some/other:verb"), some__verb);
  EXPECT_EQ(Lookup("GET", "/some/other:verb/"), nullptr);
  EXPECT_EQ(Lookup("GET", "/some/bar/foo:verb"), some__foo_verb);
  EXPECT_EQ(Lookup("GET", "/some/foo1/foo2/foo:verb"), nullptr);
  EXPECT_EQ(Lookup("GET", "/some/foo/bar:verb"), nullptr);
  EXPECT_EQ(Lookup("GET", "/other/bar/foo:verb"), other__verb);
  EXPECT_EQ(Lookup("GET", "/other/bar/foo/const:verb"), other__const_verb);
}

TEST_F(PathMatcherTest, CustomVerbMatch2) {
  MethodInfo* verb = AddGetPath("/*/*:verb");
  Build();
  EXPECT_EQ(Lookup("GET", "/some:verb/const:verb"), verb);
}

TEST_F(PathMatcherTest, CustomVerbMatch3) {
  MethodInfo* verb = AddGetPath("/foo/*");
  Build();

  // This is not custom verb since it was not configured.
  EXPECT_EQ(Lookup("GET", "/foo/other:verb"), verb);
}

TEST_F(PathMatcherTest, CustomVerbMatch4) {
  MethodInfo* a = AddGetPath("/foo/*/hello");
  Build();

  EXPECT_NE(nullptr, a);

  // last slash is before last colon.
  EXPECT_EQ(Lookup("GET", "/foo/other:verb/hello"), a);
}

TEST_F(PathMatcherTest, RejectPartialMatches) {
  MethodInfo* prefix_middle_suffix = AddGetPath("/prefix/middle/suffix");
  MethodInfo* prefix_middle = AddGetPath("/prefix/middle");
  MethodInfo* prefix = AddGetPath("/prefix");
  Build();

  EXPECT_NE(nullptr, prefix_middle_suffix);
  EXPECT_NE(nullptr, prefix_middle);
  EXPECT_NE(nullptr, prefix);

  EXPECT_EQ(Lookup("GET", "/prefix/middle/suffix"), prefix_middle_suffix);
  EXPECT_EQ(Lookup("GET", "/prefix/middle"), prefix_middle);
  EXPECT_EQ(Lookup("GET", "/prefix"), prefix);

  EXPECT_EQ(Lookup("GET", "/prefix/middle/suffix/other"), nullptr);
  EXPECT_EQ(Lookup("GET", "/prefix/middle/other"), nullptr);
  EXPECT_EQ(Lookup("GET", "/prefix/other"), nullptr);
  EXPECT_EQ(Lookup("GET", "/other"), nullptr);
}

TEST_F(PathMatcherTest, LookupReturnsNullIfMatcherEmpty) {
  Build();
  EXPECT_EQ(Lookup("GET", "a/b/blue/foo"), nullptr);
}

TEST_F(PathMatcherTest, LookupSimplePaths) {
  MethodInfo* pms = AddGetPath("/prefix/middle/suffix");
  MethodInfo* pmo = AddGetPath("/prefix/middle/othersuffix");
  MethodInfo* pos = AddGetPath("/prefix/othermiddle/suffix");
  MethodInfo* oms = AddGetPath("/otherprefix/middle/suffix");
  MethodInfo* os = AddGetPath("/otherprefix/suffix");
  Build();

  EXPECT_NE(nullptr, pms);
  EXPECT_NE(nullptr, pmo);
  EXPECT_NE(nullptr, pos);
  EXPECT_NE(nullptr, oms);
  EXPECT_NE(nullptr, os);

  EXPECT_EQ(Lookup("GET", "/prefix/not/a/path"), nullptr);
  EXPECT_EQ(Lookup("GET", "/prefix/middle"), nullptr);
  EXPECT_EQ(Lookup("GET", "/prefix/not/othermiddle"), nullptr);
  EXPECT_EQ(Lookup("GET", "/otherprefix/suffix/othermiddle"), nullptr);

  EXPECT_EQ(Lookup("GET", "/prefix/middle/suffix"), pms);
  EXPECT_EQ(Lookup("GET", "/prefix/middle/othersuffix"), pmo);
  EXPECT_EQ(Lookup("GET", "/prefix/othermiddle/suffix"), pos);
  EXPECT_EQ(Lookup("GET", "/otherprefix/middle/suffix"), oms);
  EXPECT_EQ(Lookup("GET", "/otherprefix/suffix"), os);
  EXPECT_EQ(Lookup("GET", "/otherprefix/suffix?foo=bar"), os);
}

TEST_F(PathMatcherTest, ReplacevoidForPath) {
  const std::string path = "/foo/bar";
  auto first_mock_proc = AddGetPath(path);
  EXPECT_NE(nullptr, first_mock_proc);
  // Second call should fail
  EXPECT_EQ(nullptr, AddGetPath(path));
  Build();

  // Lookup result should get the first one.
  EXPECT_EQ(Lookup("GET", path), first_mock_proc);
}

TEST_F(PathMatcherTest, AllowDuplicate) {
  MethodInfo* id = AddGetPath("/a/{id}");
  EXPECT_NE(nullptr, id);
  // Second call should fail
  EXPECT_EQ(nullptr, AddGetPath("/a/{name}"));
  Build();

  // Lookup result should get the first one.
  EXPECT_EQ(Lookup("GET", "/a/x"), id);
}

TEST_F(PathMatcherTest, DuplicatedOptions) {
  MethodInfo* get_id = AddPath("GET", "/a/{id}");
  MethodInfo* post_name = AddPath("POST", "/a/{name}");
  MethodInfo* options_id = AddPath("OPTIONS", "/a/{id}");
  EXPECT_EQ(nullptr, AddPath("OPTIONS", "/a/{name}"));
  Build();

  // Lookup result should get the first one.
  EXPECT_EQ(Lookup("OPTIONS", "/a/x"), options_id);

  EXPECT_EQ(Lookup("GET", "/a/x"), get_id);
  EXPECT_EQ(Lookup("POST", "/a/x"), post_name);
}

// If a path matches a complete branch of trie, but is longer than the branch
// (ie. the trie cannot match all the way to the end of the path), Lookup
// should return nullptr.
TEST_F(PathMatcherTest, LookupReturnsNullForOverspecifiedPath) {
  EXPECT_NE(nullptr, AddGetPath("/a/b/c"));
  EXPECT_NE(nullptr, AddGetPath("/a/b"));
  Build();
  EXPECT_EQ(Lookup("GET", "/a/b/c/d"), nullptr);
}

TEST_F(PathMatcherTest, ReturnNullvoidSharedPtrForUnderspecifiedPath) {
  EXPECT_NE(nullptr, AddGetPath("/a/b/c/d"));
  Build();
  EXPECT_EQ(Lookup("GET", "/a/b/c"), nullptr);
}

TEST_F(PathMatcherTest, DifferentHttpMethod) {
  auto ab = AddGetPath("/a/b");
  Build();
  EXPECT_NE(nullptr, ab);
  EXPECT_EQ(Lookup("GET", "/a/b"), ab);
  EXPECT_EQ(Lookup("POST", "/a/b"), nullptr);
}

}  // namespace

}  // namespace api_spec
}  // namespace istio
