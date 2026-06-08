# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Implementation of the `mixed_language_library` macro."""

load(
    "//mixed_language/internal:library.bzl",
    _mixed_language_library = "mixed_language_library",
)
load(
    "//mixed_language/internal:module_map.bzl",
    "mixed_language_internal_module_map",
)
load(
    "//mixed_language/internal:umbrella_header.bzl",
    "mixed_language_umbrella_header",
)
load("//swift:module_name.bzl", "derive_swift_module_name")
load("//swift:swift_interop_hint.bzl", "swift_interop_hint")
load("//swift:swift_library.bzl", "swift_library")

# `mixed_language_library`

def mixed_language_library(
        *,
        name,
        alwayslink = False,
        clang_copts = [],
        clang_defines = [],
        clang_srcs,
        data = [],
        enable_modules = False,
        hdrs = [],
        includes = [],
        linkopts = [],
        module_map = None,
        module_name = None,
        non_arc_srcs = [],
        package_name = None,
        private_deps = [],
        sdk_dylibs = [],
        sdk_frameworks = [],
        swift_copts = [],
        swift_defines = [],
        swift_srcs,
        swiftc_inputs = [],
        textual_hdrs = [],
        umbrella_header = None,
        weak_sdk_frameworks = [],
        deps = [],
        **kwargs):
    """Creates a mixed language library from a Clang and Swift library target \
    pair.

    Note: In the future `swift_library` will support mixed-langauge libraries.
    Once that is the case, this macro will be deprecated.

    Args:
        name: The name of the target.
        alwayslink: If true, any binary that depends (directly or indirectly) on
            this library will link in all the object files for the files listed
            in `clang_srcs` and `swift_srcs`, even if some contain no symbols
            referenced by the binary. This is useful if your code isn't
            explicitly called by code in the binary; for example, if you rely on
            runtime checks for protocol conformances added in extensions in the
            library but do not directly reference any other symbols in the
            object file that adds that conformance.
        clang_copts: The compiler flags for the clang library. These will only
            be used for the clang library. If you want them to affect the swift
            library as well, you need to pass them with `-Xcc` in `swift_copts`.
        clang_defines: Extra clang `-D` flags to pass to the compiler. They
            should be in the form `KEY=VALUE` or simply `KEY` and are passed not
            only to the compiler for this target (as `clang_copts` are) but also
            to all dependers of this target. Subject to "Make variable"
            substitution and Bourne shell tokenization.
        clang_srcs: The list of C, C++, Objective-C, or Objective-C++ sources
            for the clang library.
        data: The list of files needed by this target at runtime.

            Files and targets named in the `data` attribute will appear in the
            `*.runfiles` area of this target, if it has one. This may include
            data files needed by a binary or library, or other programs needed
            by it.
        enable_modules: Enables clang module support (via `-fmodules`). Setting
            this to `True`  will allow you to `@import` system headers and other
            targets: `@import UIKit;` `@import path_to_package_target;`.
        hdrs: The list of C, C++, Objective-C, or Objective-C++ header files
            published by this library to be included by sources in dependent
            rules. This can't include `umbrella_header`.
        includes: List of `#include`/`#import` search paths to add to this
            target and all depending targets.
        linkopts: Extra flags to pass to the linker.
        module_map: A `File` representing an existing module map that should be
            used to represent the module, or `None` (the default) if the module
            map should be generated based on `hdrs`. If this argument is
            provided, then `module_name` must also be provided.

            Warning: If a module map (whether provided here or not) is able to
            be found via an include path, it will result in duplicate module
            definition errors for downstream targets unless sandboxing or remote
            execution is used.
        module_name: The name of the Swift module being built.

            If left unspecified, the module name will be computed based on the
            target's build label, by stripping the leading `//` and replacing
            `/`, `:`, and other non-identifier characters with underscores.
        non_arc_srcs: The list of Objective-C files that are processed to create
            the library target that DO NOT use ARC. The files in this attribute
            are treated very similar to those in the `clang_srcs` attribute, but
            are compiled without ARC enabled.
        package_name: The semantic package of the Swift target being built. Targets
            with the same `package_name` can access APIs using the 'package'
            access control modifier in Swift 5.9+.
        private_deps: A list of targets that are implementation-only
            dependencies of the target being built. Libraries/linker flags from
            these dependencies will be propagated to dependent for linking, but
            artifacts/flags required for compilation (such as .swiftmodule
            files, C headers, and search paths) will not be propagated.
        sdk_dylibs: A list of of SDK `.dylib` libraries to link with. For
            instance, "libz" or "libarchive". "libc++" is included automatically
            if the binary has any C++ or Objective-C++ sources in its dependency
            tree. When linking a binary, all libraries named in that binary's
            transitive dependency graph are used.
        sdk_frameworks: A list of SDK frameworks to link with (e.g.
            "AddressBook", "QuartzCore").

            When linking a top level Apple binary, all SDK frameworks listed in
            that binary's transitive dependency graph are linked.
        swift_copts: The compiler flags for the swift library.
        swift_defines: A list of Swift defines to add to the compilation command
            line.

            Note that unlike C-family languages, Swift defines do not have
            values; they are simply identifiers that are either defined or
            undefined. So strings in this list should be simple identifiers,
            not `name=value` pairs.

            Each string is prepended with `-D` and added to the command line.
            Unlike `swift_copts`, these flags are added for the target and
            every target that depends on it, so use this attribute with caution.
            It is preferred that you add defines directly to `swift_copts`, only
            using this feature in the rare case that a library needs to
            propagate a symbol up to those that depend on it.
        swift_srcs: The sources for the swift library.
        swiftc_inputs: Additional files that are referenced using
            `$(location ...)` in attributes that support location expansion.
        textual_hdrs: The list of C, C++, Objective-C, or Objective-C++ files
            that are included as headers by source files in this rule or by
            users of this library. Unlike `hdrs`, these will not be compiled
            separately from the sources.
        umbrella_header: A `File` representing an existing umbrella header that
            should be used in the generated module map or is used in the custom
            module map, or `None` (the default) if the umbrella header should be
            generated based on `hdrs`. A symlink to this header is added to an
            include path such that `#import <ModuleName/ModuleName.h>` works for
            this and downstream targets.
        weak_sdk_frameworks: A list of SDK frameworks to weakly link with. For
            instance, "MediaAccessibility". In difference to regularly linked
            SDK frameworks, symbols from weakly linked frameworks do not cause
            an error if they are not present at runtime.
        deps: A list of targets that are dependencies of the target being built.
        **kwargs: Additional arguments to pass to the underlying clang and swift
            library targets.
    """
    aspect_hints = kwargs.pop("aspect_hints", [])
    features = kwargs.pop("features", [])
    tags = kwargs.pop("tags", [])
    testonly = kwargs.pop("testonly", None)
    visibility = kwargs.pop("visibility", None)

    internal_tags = (
        ((["manual"] + tags) if "manual" not in tags else tags) +
        # Allows for easy query filtering of the internal targets
        ["mixed_language_library_internal"]
    )

    if "generates_header" in kwargs:
        fail(
            """\
'generates_header' is an invalid attribute for 'mixed_language_library'.\
""",
            attr = "generates_header",
        )
    if "copts" in kwargs:
        fail(
            """\
Use 'clang_copts' and/or 'swift_copts' to set copts with \
'mixed_language_library'.\
""",
            attr = "copts",
        )
    if "defines" in kwargs:
        fail(
            """\
Use 'clang_defines' and/or 'swift_defines' to set defines for \
'mixed_language_library'.\
""",
            attr = "defines",
        )
    if "implementation_deps" in kwargs:
        fail(
            """\
'implementation_deps' is an invalid attribute for 'mixed_language_library'. \
Use 'private_deps' to set implementation deps.\
""",
            attr = "implementation_deps",
        )

    if not clang_srcs and not hdrs:
        fail(
            """\
'mixed_language_library' requires either 'clang_srcs' and/or 'hdrs' to be \
non-empty. If this is not a mixed language Swift library, use `swift_library` \
instead.\
""",
            attr = "clang_srcs",
        )
    if not swift_srcs:
        fail(
            """\
'mixed_language_library' requires 'swift_srcs' to be non-empty. If this is not \
a mixed language Swift library, use a clang only library rule like \
`cc_library` or `objc_library` instead.\
""",
            attr = "swift_srcs",
        )

    if not module_name:
        module_name = derive_swift_module_name(native.package_name(), name)

    if not module_map:
        internal_modulemap_name = name + "_modulemap"
        mixed_language_internal_module_map(
            name = internal_modulemap_name,
            hdrs = hdrs,
            module_map_name = name,
            module_name = module_name,
            tags = internal_tags,
            textual_hdrs = textual_hdrs,
            # We can't use a generated umbrella header here, it results in
            # duplicated headers being found. There is probvably a way to make
            # that work, but I don't know of it right now.
            umbrella_header = umbrella_header,
        )
        module_map = ":" + internal_modulemap_name
    elif not module_name:
        fail(
            "If 'module_map' is provided, 'module_name' must also be provided.",
            attr = "module_name",
        )

    if not umbrella_header:
        internal_umbrella_header_name = name + "_umbrella_header"
        mixed_language_umbrella_header(
            name = internal_umbrella_header_name,
            hdrs = hdrs,
            module_name = module_name,
            tags = internal_tags,
        )
        umbrella_header = ":" + internal_umbrella_header_name

        # We need both targets to depend on this to get the includes path that
        # lets `#import <ModuleName/ModuleName.h>` work, since minimally it
        # might be in the `-Swift.h` header.
        private_deps = private_deps + [umbrella_header]

    # The umbrella header is a public header
    adjusted_hdrs = [umbrella_header] + hdrs

    # `_headers` is the public interface of the `_clang` target, to be consumed
    # by the `_swift` target. It won't be propagated to dependers of the mixed
    # language library. The `_swift_interop` aspect hint allows the `_swift`
    # target to import the module map for the `_clang` target.
    internal_swift_interop_name = name + "_swift_interop"
    swift_interop_hint(
        name = internal_swift_interop_name,
        module_map = module_map,
        module_name = module_name,
    )
    headers_library_name = name + "_headers"
    native.objc_library(
        name = headers_library_name,
        hdrs = adjusted_hdrs,
        aspect_hints = aspect_hints + [":" + internal_swift_interop_name],
        defines = clang_defines,
        features = features,
        includes = includes,
        tags = internal_tags,
        textual_hdrs = textual_hdrs,
        **kwargs
    )

    swift_library_name = name + "_swift"
    swift_library(
        name = swift_library_name,
        srcs = swift_srcs,
        alwayslink = alwayslink,
        aspect_hints = aspect_hints,
        copts = ["-import-underlying-module"] + swift_copts,
        defines = swift_defines,
        # We generate a module map in `_mixed_language_library` instead
        features = features + ["swift.no_generated_module_map"],
        generates_header = True,
        # TODO: Allow customizing `generated_header_name`
        # This (using module name instead of target name) should be the default
        # for `swift_library`, but is a breaking change
        generated_header_name = module_name + "-Swift.h",
        linkopts = linkopts,
        module_name = module_name,
        package_name = package_name,
        private_deps = private_deps,
        swiftc_inputs = swiftc_inputs,
        tags = internal_tags,
        testonly = testonly,
        # The `_headers` target will have headers set for inputs, set
        # `includes`, and set `SwiftInfo.clang.modulemap` (via the
        # `_swift_interop` target). It needs to be a private dep to prevent from
        # propagating outside of these targets. The final
        # `_mixed_language_library` target will propagate the umbrella header
        # and extended module map.
        #
        # This would use `private_deps`, but that doesn't propagate `includes`
        # since that could impact the generated header. We use `deps` instead,
        # which is fine because `mixed_language_library` creates a new
        # `SwiftInfo` which will ignore the `swift_interop_hint` set on the
        # `_headers` target.
        deps = [":" + headers_library_name] + deps,
        **kwargs
    )

    # TODO: Should this support `cc_library`, and if so, how do we choose?
    # We can't look at the `clang_srcs` attribute because it might be a
    # `select`.
    clang_library_name = name + "_clang"
    native.objc_library(
        name = clang_library_name,
        srcs = clang_srcs,
        alwayslink = alwayslink,
        hdrs = adjusted_hdrs,
        non_arc_srcs = non_arc_srcs,
        # `internal_swift_interop_name` isn't needed here because
        # `_mixed_language_library` will explciitly set the module name. We set
        # it here for rules_xcodeproj or other aspects to be able to assign a
        # sane module name to the target. Since we use the target for the
        # `_headers` target, this has minimal additional overhead.
        aspect_hints = aspect_hints + [":" + internal_swift_interop_name],
        copts = clang_copts,
        defines = clang_defines,
        enable_modules = enable_modules,
        features = features,
        includes = includes,
        # The `_swift` target is an implementation dep because
        # `_mixed_language_library` will propagate the final `SwiftInfo`
        implementation_deps = [":" + swift_library_name] + private_deps,
        linkopts = linkopts,
        sdk_dylibs = sdk_dylibs,
        sdk_frameworks = sdk_frameworks,
        tags = internal_tags,
        testonly = testonly,
        textual_hdrs = textual_hdrs,
        weak_sdk_frameworks = weak_sdk_frameworks,
        deps = deps,
        **kwargs
    )

    # `_mixed_language_library` creates an extended modulemap that includes
    # the `.Swift` submodule. It returns a new `SwiftInfo` that has that
    # module map set so downstream Swift targets will see it. If the
    # `swift.propagate_generated_module_map` feature is set, then an include
    # path is set in `CcInfo.compilation+context` so that downstream Clang
    # targets will see it via implicit search.
    _mixed_language_library(
        name = name,
        aspect_hints = aspect_hints,
        clang_target = ":" + clang_library_name,
        data = data,
        features = features,
        module_map = module_map,
        module_name = module_name,
        swift_target = ":" + swift_library_name,
        tags = tags,
        testonly = testonly,
        umbrella_header = umbrella_header,
        visibility = visibility,
        # We collect the same deps as the `_swift` target in order to propagate
        # transitive `SwiftInfo`. It's inefficent to try to do that from the
        # `SwiftInfo` of the `_swift` target.
        deps = deps,
    )
