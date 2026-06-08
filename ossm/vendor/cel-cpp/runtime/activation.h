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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_ACTIVATION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/function_descriptor.h"
#include "common/value.h"
#include "runtime/activation_interface.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "runtime/internal/attribute_matcher.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace runtime_internal {
class ActivationAttributeMatcherAccess;
}

// Thread-compatible implementation of a CEL Activation.
//
// Values can either be provided eagerly or via a provider.
class Activation final : public ActivationInterface {
 public:
  // Definition for value providers.
  using ValueProvider =
      absl::AnyInvocable<absl::StatusOr<absl::optional<Value>>(
          absl::string_view, const google::protobuf::DescriptorPool* absl_nonnull,
          google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull)>;

  Activation() = default;

  // Move only.
  Activation(Activation&& other);

  Activation& operator=(Activation&& other);

  // Implements ActivationInterface.
  absl::StatusOr<bool> FindVariable(
      absl::string_view name,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override;
  using ActivationInterface::FindVariable;

  std::vector<FunctionOverloadReference> FindFunctionOverloads(
      absl::string_view name) const override;

  absl::Span<const cel::AttributePattern> GetUnknownAttributes()
      const override {
    return unknown_patterns_;
  }

  absl::Span<const cel::AttributePattern> GetMissingAttributes()
      const override {
    return missing_patterns_;
  }

  // Bind a value to a named variable.
  //
  // Returns false if the entry for name was overwritten.
  bool InsertOrAssignValue(absl::string_view name, Value value);

  // Bind a provider to a named variable. The result of the provider may be
  // memoized by the activation.
  //
  // Returns false if the entry for name was overwritten.
  bool InsertOrAssignValueProvider(absl::string_view name,
                                   ValueProvider provider);

  void AddUnknownPattern(cel::AttributePattern pattern) {
    unknown_patterns_.push_back(std::move(pattern));
  }

  void SetUnknownPatterns(std::vector<cel::AttributePattern> patterns) {
    unknown_patterns_ = std::move(patterns);
  }

  void AddMissingPattern(cel::AttributePattern pattern) {
    missing_patterns_.push_back(std::move(pattern));
  }

  void SetMissingPatterns(std::vector<cel::AttributePattern> patterns) {
    missing_patterns_ = std::move(patterns);
  }

  // Returns true if the function was inserted (no other registered function has
  // a matching descriptor).
  bool InsertFunction(const cel::FunctionDescriptor& descriptor,
                      std::unique_ptr<cel::Function> impl);

 private:
  struct ValueEntry {
    // If provider is present, then access must be synchronized to maintain
    // thread-compatible semantics for the lazily provided value.
    absl::optional<Value> value;
    absl::optional<ValueProvider> provider;
  };

  struct FunctionEntry {
    std::unique_ptr<cel::FunctionDescriptor> descriptor;
    std::unique_ptr<cel::Function> implementation;
  };

  friend class runtime_internal::ActivationAttributeMatcherAccess;

  void SetAttributeMatcher(const runtime_internal::AttributeMatcher* matcher) {
    attribute_matcher_ = matcher;
  }

  void SetAttributeMatcher(
      std::unique_ptr<const runtime_internal::AttributeMatcher> matcher) {
    owned_attribute_matcher_ = std::move(matcher);
    attribute_matcher_ = owned_attribute_matcher_.get();
  }

  const runtime_internal::AttributeMatcher* absl_nullable GetAttributeMatcher()
      const override {
    return attribute_matcher_;
  }

  friend void swap(Activation& a, Activation& b) {
    using std::swap;
    swap(a.values_, b.values_);
    swap(a.functions_, b.functions_);
    swap(a.unknown_patterns_, b.unknown_patterns_);
    swap(a.missing_patterns_, b.missing_patterns_);
  }

  // Internal getter for provided values.
  // Assumes entry for name is present and is a provided value.
  // Handles synchronization for caching the provided value.
  absl::StatusOr<bool> ProvideValue(
      absl::string_view name,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;

  // mutex_ used for safe caching of provided variables
  mutable absl::Mutex mutex_;
  mutable absl::flat_hash_map<std::string, ValueEntry> values_;

  std::vector<cel::AttributePattern> unknown_patterns_;
  std::vector<cel::AttributePattern> missing_patterns_;

  const runtime_internal::AttributeMatcher* attribute_matcher_ = nullptr;
  std::unique_ptr<const runtime_internal::AttributeMatcher>
      owned_attribute_matcher_;

  absl::flat_hash_map<std::string, std::vector<FunctionEntry>> functions_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_ACTIVATION_H_
