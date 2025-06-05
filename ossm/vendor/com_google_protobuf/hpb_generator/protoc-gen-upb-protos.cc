// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google LLC.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/hpb/gen_enums.h"
#include "google/protobuf/compiler/hpb/gen_extensions.h"
#include "google/protobuf/compiler/hpb/gen_messages.h"
#include "google/protobuf/compiler/hpb/gen_utils.h"
#include "google/protobuf/compiler/hpb/names.h"
#include "google/protobuf/compiler/hpb/output.h"
#include "google/protobuf/compiler/plugin.h"
#include "google/protobuf/descriptor.h"

namespace google::protobuf::hpb_generator {
namespace {

namespace protoc = ::google::protobuf::compiler;
namespace protobuf = ::proto2;
using FileDescriptor = ::google::protobuf::FileDescriptor;
using google::protobuf::Edition;

void WriteSource(const protobuf::FileDescriptor* file, Output& output,
                 bool fasttable_enabled, bool strip_feature_includes);
void WriteHeader(const protobuf::FileDescriptor* file, Output& output,
                 bool strip_feature_includes);
void WriteForwardingHeader(const protobuf::FileDescriptor* file,
                           Output& output);
void WriteMessageImplementations(const protobuf::FileDescriptor* file,
                                 Output& output);
void WriteTypedefForwardingHeader(
    const protobuf::FileDescriptor* file,
    const std::vector<const protobuf::Descriptor*>& file_messages,
    Output& output);
void WriteHeaderMessageForwardDecls(const protobuf::FileDescriptor* file,
                                    Output& output,
                                    bool strip_feature_includes);

class Generator : public protoc::CodeGenerator {
 public:
  ~Generator() override = default;
  bool Generate(const protobuf::FileDescriptor* file,
                const std::string& parameter, protoc::GeneratorContext* context,
                std::string* error) const override;
  uint64_t GetSupportedFeatures() const override {
    return Feature::FEATURE_PROTO3_OPTIONAL |
           Feature::FEATURE_SUPPORTS_EDITIONS;
  }
  Edition GetMinimumEdition() const override { return Edition::EDITION_PROTO2; }
  Edition GetMaximumEdition() const override { return Edition::EDITION_2023; }
};

bool Generator::Generate(const protobuf::FileDescriptor* file,
                         const std::string& parameter,
                         protoc::GeneratorContext* context,
                         std::string* error) const {
  bool fasttable_enabled = false;
  bool strip_nonfunctional_codegen = false;
  std::vector<std::pair<std::string, std::string>> params;
  google::protobuf::compiler::ParseGeneratorParameter(parameter, &params);

  for (const auto& pair : params) {
    if (pair.first == "fasttable") {
      fasttable_enabled = true;
    } else if (pair.first == "experimental_strip_nonfunctional_codegen") {
      strip_nonfunctional_codegen = true;
    } else {
      *error = "Unknown parameter: " + pair.first;
      return false;
    }
  }

  // Write model.upb.fwd.h
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> output_stream(
      context->Open(ForwardingHeaderFilename(file)));
  Output forwarding_header_output(output_stream.get());
  WriteForwardingHeader(file, forwarding_header_output);

  // Write model.upb.proto.h
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> header_output_stream(
      context->Open(CppHeaderFilename(file)));
  Output header_output(header_output_stream.get());
  WriteHeader(file, header_output, strip_nonfunctional_codegen);

  // Write model.upb.proto.cc
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> cc_output_stream(
      context->Open(CppSourceFilename(file)));
  Output cc_output(cc_output_stream.get());
  WriteSource(file, cc_output, fasttable_enabled, strip_nonfunctional_codegen);
  return true;
}

// The forwarding header defines Access/Proxy/CProxy for message classes
// used to include when referencing dependencies to prevent transitive
// dependency headers from being included.
void WriteForwardingHeader(const protobuf::FileDescriptor* file,
                           Output& output) {
  EmitFileWarning(file, output);
  output(
      R"cc(
#ifndef $0_UPB_FWD_H_
#define $0_UPB_FWD_H_
      )cc",
      ToPreproc(file->name()));
  output("\n");
  for (int i = 0; i < file->public_dependency_count(); ++i) {
    output("#include \"$0\"\n",
           ForwardingHeaderFilename(file->public_dependency(i)));
  }
  if (file->public_dependency_count() > 0) {
    output("\n");
  }
  const std::vector<const protobuf::Descriptor*> this_file_messages =
      SortedMessages(file);
  WriteTypedefForwardingHeader(file, this_file_messages, output);
  output("#endif  /* $0_UPB_FWD_H_ */\n", ToPreproc(file->name()));
}

void WriteHeader(const protobuf::FileDescriptor* file, Output& output,
                 bool strip_feature_includes) {
  EmitFileWarning(file, output);
  output(
      R"cc(
#ifndef $0_HPB_PROTO_H_
#define $0_HPB_PROTO_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

#include "google/protobuf/hpb/repeated_field.h"
      )cc",
      ToPreproc(file->name()));

  // Import headers for proto public dependencies.
  for (int i = 0; i < file->public_dependency_count(); i++) {
    if (i == 0) {
      output("// Public Imports.\n");
    }
    output("#include \"$0\"\n", CppHeaderFilename(file->public_dependency(i)));
    if (i == file->public_dependency_count() - 1) {
      output("\n");
    }
  }

  output("#include \"upb/port/def.inc\"\n");

  const std::vector<const protobuf::Descriptor*> this_file_messages =
      SortedMessages(file);
  const std::vector<const protobuf::FieldDescriptor*> this_file_exts =
      SortedExtensions(file);

  if (!this_file_messages.empty()) {
    output("\n");
  }

  WriteHeaderMessageForwardDecls(file, output, strip_feature_includes);
  WriteStartNamespace(file, output);

  std::vector<const protobuf::EnumDescriptor*> this_file_enums =
      SortedEnums(file);

  // Write Class and Enums.
  WriteEnumDeclarations(this_file_enums, output);
  output("\n");

  for (auto message : this_file_messages) {
    WriteMessageClassDeclarations(message, this_file_exts, this_file_enums,
                                  output);
  }
  output("\n");

  WriteExtensionIdentifiersHeader(this_file_exts, output);
  output("\n");

  WriteEndNamespace(file, output);

  output("\n#include \"upb/port/undef.inc\"\n\n");
  // End of "C" section.

  output("#endif  /* $0_HPB_PROTO_H_ */\n", ToPreproc(file->name()));
}

// Writes a .upb.cc source file.
void WriteSource(const protobuf::FileDescriptor* file, Output& output,
                 bool fasttable_enabled, bool strip_feature_includes) {
  EmitFileWarning(file, output);

  output(
      R"cc(
#include <stddef.h>
#include "absl/strings/string_view.h"
#include "$0"
      )cc",
      CppHeaderFilename(file));

  for (int i = 0; i < file->dependency_count(); i++) {
    if (strip_feature_includes &&
        compiler::IsKnownFeatureProto(file->dependency(i)->name())) {
      // Strip feature imports for editions codegen tests.
      continue;
    }
    output("#include \"$0\"\n", CppHeaderFilename(file->dependency(i)));
  }
  output("#include \"upb/port/def.inc\"\n");

  WriteStartNamespace(file, output);
  WriteMessageImplementations(file, output);
  const std::vector<const protobuf::FieldDescriptor*> this_file_exts =
      SortedExtensions(file);
  WriteExtensionIdentifiers(this_file_exts, output);
  WriteEndNamespace(file, output);

  output("#include \"upb/port/undef.inc\"\n\n");
}

void WriteMessageImplementations(const protobuf::FileDescriptor* file,
                                 Output& output) {
  const std::vector<const protobuf::FieldDescriptor*> file_exts =
      SortedExtensions(file);
  const std::vector<const protobuf::Descriptor*> this_file_messages =
      SortedMessages(file);
  for (auto message : this_file_messages) {
    WriteMessageImplementation(message, file_exts, output);
  }
}

void WriteTypedefForwardingHeader(
    const protobuf::FileDescriptor* file,
    const std::vector<const protobuf::Descriptor*>& file_messages,
    Output& output) {
  WriteStartNamespace(file, output);

  // Forward-declare types defined in this file.
  for (auto message : file_messages) {
    output(
        R"cc(
          class $0;
          namespace internal {
          class $0Access;
          class $0Proxy;
          class $0CProxy;
          }  // namespace internal
        )cc",
        ClassName(message));
  }
  output("\n");
  WriteEndNamespace(file, output);
}

/// Writes includes for upb C minitables and fwd.h for transitive typedefs.
void WriteHeaderMessageForwardDecls(const protobuf::FileDescriptor* file,
                                    Output& output,
                                    bool strip_feature_includes) {
  // Import forward-declaration of types defined in this file.
  output("#include \"$0\"\n", UpbCFilename(file));
  output("#include \"$0\"\n", ForwardingHeaderFilename(file));
  // Import forward-declaration of types in dependencies.
  for (int i = 0; i < file->dependency_count(); ++i) {
    if (strip_feature_includes &&
        compiler::IsKnownFeatureProto(file->dependency(i)->name())) {
      // Strip feature imports for editions codegen tests.
      continue;
    }
    output("#include \"$0\"\n", ForwardingHeaderFilename(file->dependency(i)));
  }
  output("\n");
}

}  // namespace
}  // namespace protobuf
}  // namespace google::hpb_generator

int main(int argc, char** argv) {
  google::protobuf::hpb_generator::Generator generator_cc;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator_cc);
}
