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
//
// Interfaces for runtime concepts.

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/ast.h"
#include "base/type_provider.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "runtime/activation_interface.h"
#include "runtime/runtime_issue.h"

namespace cel {

namespace runtime_internal {
class RuntimeFriendAccess;
}  // namespace runtime_internal

// Representation of an evaluable CEL expression.
//
// See Runtime below for creating new programs.
class Program {
 public:
  virtual ~Program() = default;

  // Evaluate the program.
  //
  // Non-recoverable errors (i.e. outside of CEL's notion of an error) are
  // returned as a non-ok absl::Status. These are propagated immediately and do
  // not participate in CEL's notion of error handling.
  //
  // CEL errors are represented as result with an Ok status and a held
  // cel::ErrorValue result.
  //
  // Activation manages instances of variables available in the cel expression's
  // environment.
  //
  // The memory manager determines the lifecycle requirements of the returned
  // value. The most common choices are:
  //  - cel::MemoryManagerRef::ReferenceCounting(): created values are allocated
  //  on the heap
  //    and managed by a reference count. Destructor is called when reference
  //    count is 0.
  //  - cel::extensions::ProtoMemoryManager instance: created values are
  //    allocated on the backing protobuf Arena. Destructors for allocated
  //    objects are called on destruction of the Arena. Note: instances may
  //    still allocate additional memory on the heap e.g. a vector's storage
  //    may still be on the global heap.
  //
  //  For consistency, users should use the same memory manager to create values
  //  in the activation and for Program evaluation.
  virtual absl::StatusOr<Value> Evaluate(const ActivationInterface& activation,
                                         ValueManager& value_factory) const = 0;

  virtual const TypeProvider& GetTypeProvider() const = 0;
};

// Representation for a traceable CEL expression.
//
// Implementations provide an additional Trace method that evaluates the
// expression and invokes a callback allowing callers to inspect intermediate
// state during evaluation.
class TraceableProgram : public Program {
 public:
  // EvaluationListener may be provided to an EvaluateWithCallback call to
  // inspect intermediate values during evaluation.
  //
  // The callback is called on after every program step that corresponds
  // to an AST expression node. The value provided is the top of the value
  // stack, corresponding to the result of evaluating the given sub expression.
  //
  // A returning a non-ok status stops evaluation and forwards the error.
  using EvaluationListener = absl::AnyInvocable<absl::Status(
      int64_t expr_id, const Value&, ValueManager&)>;

  // Evaluate the Program plan with a Listener.
  //
  // The given callback will be invoked after evaluating any program step
  // that corresponds to an AST node in the planned CEL expression.
  //
  // If the callback returns a non-ok status, evaluation stops and the Status
  // is forwarded as the result of the EvaluateWithCallback call.
  virtual absl::StatusOr<Value> Trace(const ActivationInterface&,
                                      EvaluationListener evaluation_listener,
                                      ValueManager& value_factory) const = 0;
};

// Interface for a CEL runtime.
//
// Manages the state necessary to generate Programs.
//
// Runtime instances should be created from a RuntimeBuilder rather than
// instantiated directly.
class Runtime {
 public:
  struct CreateProgramOptions {
    // Optional output for collecting issues encountered while planning.
    // If non-null, vector is cleared and encountered issues are added.
    std::vector<RuntimeIssue>* issues = nullptr;
  };

  virtual ~Runtime() = default;

  absl::StatusOr<std::unique_ptr<Program>> CreateProgram(
      std::unique_ptr<cel::Ast> ast) const {
    return CreateProgram(std::move(ast), CreateProgramOptions{});
  }

  virtual absl::StatusOr<std::unique_ptr<Program>> CreateProgram(
      std::unique_ptr<cel::Ast> ast,
      const CreateProgramOptions& options) const = 0;

  absl::StatusOr<std::unique_ptr<TraceableProgram>> CreateTraceableProgram(
      std::unique_ptr<cel::Ast> ast) const {
    return CreateTraceableProgram(std::move(ast), CreateProgramOptions{});
  }

  virtual absl::StatusOr<std::unique_ptr<TraceableProgram>>
  CreateTraceableProgram(std::unique_ptr<cel::Ast> ast,
                         const CreateProgramOptions& options) const = 0;

  virtual const TypeProvider& GetTypeProvider() const = 0;

 private:
  friend class runtime_internal::RuntimeFriendAccess;

  virtual NativeTypeId GetNativeTypeId() const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_H_
