// Copyright 2024 Google LLC
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

#include "compiler/compiler_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "checker/type_checker_builder_factory.h"
#include "checker/validation_result.h"
#include "common/source.h"
#include "compiler/compiler.h"
#include "internal/status_macros.h"
#include "parser/parser.h"
#include "parser/parser_interface.h"
#include "google/protobuf/descriptor.h"

namespace cel {

namespace {

class CompilerImpl : public Compiler {
 public:
  CompilerImpl(std::unique_ptr<TypeChecker> type_checker,
               std::unique_ptr<Parser> parser)
      : type_checker_(std::move(type_checker)), parser_(std::move(parser)) {}

  absl::StatusOr<ValidationResult> Compile(
      absl::string_view expression,
      absl::string_view description) const override {
    CEL_ASSIGN_OR_RETURN(auto source,
                         cel::NewSource(expression, std::string(description)));
    CEL_ASSIGN_OR_RETURN(auto ast, parser_->Parse(*source));
    CEL_ASSIGN_OR_RETURN(ValidationResult result,
                         type_checker_->Check(std::move(ast)));

    result.SetSource(std::move(source));
    return result;
  }

  const TypeChecker& GetTypeChecker() const override { return *type_checker_; }
  const Parser& GetParser() const override { return *parser_; }

 private:
  std::unique_ptr<TypeChecker> type_checker_;
  std::unique_ptr<Parser> parser_;
};

class CompilerBuilderImpl : public CompilerBuilder {
 public:
  CompilerBuilderImpl(std::unique_ptr<TypeCheckerBuilder> type_checker_builder,
                      std::unique_ptr<ParserBuilder> parser_builder)
      : type_checker_builder_(std::move(type_checker_builder)),
        parser_builder_(std::move(parser_builder)) {}

  absl::Status AddLibrary(CompilerLibrary library) override {
    if (!library.id.empty()) {
      auto [it, inserted] = library_ids_.insert(library.id);

      if (!inserted) {
        return absl::AlreadyExistsError(
            absl::StrCat("library already exists: ", library.id));
      }
    }

    if (library.configure_checker) {
      CEL_RETURN_IF_ERROR(type_checker_builder_->AddLibrary({
          .id = library.id,
          .configure = std::move(library.configure_checker),
      }));
    }
    if (library.configure_parser) {
      CEL_RETURN_IF_ERROR(parser_builder_->AddLibrary({
          .id = library.id,
          .configure = std::move(library.configure_parser),
      }));
    }
    return absl::OkStatus();
  }

  absl::Status AddLibrarySubset(CompilerLibrarySubset subset) override {
    if (subset.library_id.empty()) {
      return absl::InvalidArgumentError("library id must not be empty");
    }
    std::string library_id = subset.library_id;

    auto [it, inserted] = subsets_.insert(library_id);
    if (!inserted) {
      return absl::AlreadyExistsError(
          absl::StrCat("library subset already exists for: ", library_id));
    }

    if (subset.should_include_macro) {
      CEL_RETURN_IF_ERROR(parser_builder_->AddLibrarySubset({
          library_id,
          std::move(subset.should_include_macro),
      }));
    }
    if (subset.should_include_overload) {
      CEL_RETURN_IF_ERROR(type_checker_builder_->AddLibrarySubset(
          {library_id, std::move(subset.should_include_overload)}));
    }
    return absl::OkStatus();
  }

  ParserBuilder& GetParserBuilder() override { return *parser_builder_; }
  TypeCheckerBuilder& GetCheckerBuilder() override {
    return *type_checker_builder_;
  }

  absl::StatusOr<std::unique_ptr<Compiler>> Build() override {
    CEL_ASSIGN_OR_RETURN(auto parser, parser_builder_->Build());
    CEL_ASSIGN_OR_RETURN(auto type_checker, type_checker_builder_->Build());
    return std::make_unique<CompilerImpl>(std::move(type_checker),
                                          std::move(parser));
  }

 private:
  std::unique_ptr<TypeCheckerBuilder> type_checker_builder_;
  std::unique_ptr<ParserBuilder> parser_builder_;

  absl::flat_hash_set<std::string> library_ids_;
  absl::flat_hash_set<std::string> subsets_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<CompilerBuilder>> NewCompilerBuilder(
    std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool,
    CompilerOptions options) {
  if (descriptor_pool == nullptr) {
    return absl::InvalidArgumentError("descriptor_pool must not be null");
  }
  CEL_ASSIGN_OR_RETURN(auto type_checker_builder,
                       CreateTypeCheckerBuilder(std::move(descriptor_pool),
                                                options.checker_options));
  auto parser_builder = NewParserBuilder(options.parser_options);

  return std::make_unique<CompilerBuilderImpl>(std::move(type_checker_builder),
                                               std::move(parser_builder));
}

}  // namespace cel
