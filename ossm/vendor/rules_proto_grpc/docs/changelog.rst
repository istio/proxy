:author: rules_proto_grpc
:description: Changelog for the rules_proto_grpc Bazel rules
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Changelog, Changes, History


Changelog
=========

4.6.0
-----

General
*******

- Fixed incompatibility with Bazel 7 for the C, C++ and Objective-C rules.
  `#298 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/298>`__

Rust
****

- **Breaking change**: The ``preserve_proto_field_names`` option is no longer set on the Serde
  plugin by default, as it cannot then be disabled. If you need this option, set if manually with
  the ``options`` attr.
  `#297 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/297>`__
- Disabled Clippy lints in generated code.
  `#296 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/296>`__


4.5.0
-----

General
*******

- Updated grpc to 1.54.1
- Updated ``rules_proto`` to 5.3.0-21.7
- Fixed passing extra options to the ``grpc-gateway`` plugin.
  `#258 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/258>`__
- Removed header files from runfiles of `cpp_grpc_library`.
  `#262 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/262>`__
- Added a path conversion from snake_case to dashed-case.
  `#274 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/274>`__
- Fixed missing env var in documentation.
  `#279 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/279>`__

C++
***

- Added support for ``NO_PREFIX`` output mode.
  `#276 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/276>`__

C#/F#
*****

- Updated gRPC to 2.53.0

Go
**

- Updated ``rules_go`` to 0.39.1

Python
******

- Added support for passing ``data`` attr to Python library rules.
  `#257 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/257>`__

Ruby
****

- Updated ``rules_ruby`` to latest

Rust
****

- **Major change**: Replaced Rust protobuf and gRPC libraries with Prost and Tonic respectively. See
  the Rust rules documentation for examples of how this change can be adopted.
  `#265 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/265>`__


4.4.0
-----

General
*******

- Increased minimum supported Bazel version from 5.0.0 to 5.3.0.
  `#230 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/230>`__
- Added support for param file for excess arguments, which allows for longer commands lines without
  failure
- Fixed Windows incompatibility due to test workspace containing quote character in path
- The `proto_compile` function is now exported in the public `defs.bzl` for use in external rules
- Added static release assets generation, which will change the format of the download URL to use in
  your WORKSPACE. See the sample installation docs for the new URL

Go
**

- Updated ``github.com/envoyproxy/protoc-gen-validate`` to 1.0.0

grpc-gateway
************

-  **WORKSPACE update needed**: Renamed ``grpc-gateway`` repository name from
  ``grpc_ecosystem_grpc_gateway`` to ``com_github_grpc_ecosystem_grpc_gateway_v2``, to match the
  naming used by Gazelle. You may need to update your WORKSPACE file to use the new name

Objective-C
***********

- Fixed expected naming of output files for proto files containing numbers in file name.
  `#253 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/253>`__


4.3.0
-----

General
*******

- Updated protobuf to 21.10
- Updated grpc to 1.51.0
- Updated ``rules_proto`` to 5.3.0-21.5
- Updated ``bazel_skylib`` to 1.3.0
- Added support for paths to proto files that contain spaces or other special characters
- Added forwarding of all standard Bazel rule attributes for library macros
- Added support for providing plugin-specific environment variables

Buf
***

- Updated Buf plugins to v1.9.0

C
*

- **WORKSPACE update needed**: The upb version is now sourced from gRPC dependencies to prevent
  version skew in mixed C and C++ workspaces. See the example workspaces for the new template

C#/F#
*****

- Updated gRPC to 2.50.0

Go
**

- Updated ``google.golang.org/protobuf`` to 1.28.1
- Updated ``rules_go`` to 0.36.0
- Updated ``github.com/envoyproxy/protoc-gen-validate`` to 0.9.0

grpc-gateway
************

- Updated ``grpc-gateway`` to 2.14.0

gRPC-Web
********

- Added support for M1 builds of grpc-web
- Updated ``grpc-web`` to 1.4.2

Java
****

- Updated ``rules_jvm_external`` to 4.5

JavaScript
**********

- Updated ``google-protobuf`` to 3.21.2
- Updated ``@grpc/grpc-js`` to 1.7.3
- Updated ``rules_nodejs`` to 5.7.1

Python
******

- Updated ``rules_python`` to 0.15.0
- Updated ``grpclib`` to 0.4.3
- **WORKSPACE update needed**: The Python dependencies have moved from ``pip_install`` to
  ``pip_parse``, as advised by ``rules_python`` authors. See the example workspaces for the new
  template, which is only necessary if you are using grpclib
- Removed subpar dependency

Ruby
****

- Updated ``google-protobuf`` to 3.21.9
- Updated ``grpc`` to 1.50.0

Rust
****

- Updated ``rules_rust`` to 0.14.0

Scala
*****

- Update ScalaPB to 0.11.12
- Updated ``rules_scala`` to latest

Swift
*****

- Updated ``rules_swift`` to 1.4.0


4.2.0
-----

General
*******

- Updated protobuf to 21.5
- Updated grpc to 1.48.0
- Updated zlib to 1.2.12
- Switched default ``use_built_in_shell_environment`` to ``True`` .
  `#182 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/182>`__
- Bumped minimum Bazel version to 5.0.0
- Updated ``bazel_skylib`` to 1.2.1
- Added section to the documentation on overriding dependencies
- Fixed compilation failure when using a mix of plugins that output directories and files

Buf
***

- Updated Buf plugins to v1.7.0
- Added support for M1/arm64

C++
***

- **WORKSPACE update needed**: You now need to load ``grpc_extra_deps`` in your WORKSPACE file. See
  the example workspaces for the new template

C#/F#
*****

- **Breaking change**: The C# and F# rules have switched from using the deprecated ``Grpc.Core`` to
  the new ``Grpc.Net.Client`` and ``Grpc.AspNetCore``
- Updated gRPC to 2.47.0
- Updated ``rules_dotnet`` to latest
- Updated ``FSharp.Core`` to 6.0.5
- Updated ``Protobuf.FSharp`` to 0.2.0
- Updated ``grpc-fsharp`` to 0.2.0

Docs
****

- Updated ``protoc-gen-doc`` to 1.5.1

Go
**

- Updated ``rules_go`` to 0.34.0
- Updated ``gazelle`` to 0.26.0
- Updated ``protoc-gen-validate`` to 0.6.7

grpc-gateway
************

- Updated ``grpc-gateway`` to 2.11.3

gRPC-Web
********

- Updated ``grpc-web`` to 1.3.1

JavaScript
**********

- Updated ``rules_nodejs`` to 5.5.2
- Moved to ``protocolbuffers/protobuf-javascript``
- Updated ``@grpc/grpc-js`` to 1.6.7
- **WORKSPACE update needed**: The ``build_bazel_rules_nodejs_dependencies`` rule needs to be added
  to your WORKSPACE
- TypeScript support is currently somewhat broken, see `here <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/194>`__.
  This is not a change from 4.1.0

Objective-C
***********

- Fixed expected naming of output files for proto files containing dash in file name.
  `#177 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/177>`__
- **WORKSPACE update needed**: You now need to load ``grpc_extra_deps`` in your WORKSPACE file. See
  the example workspaces for the new template

Python
******

- Updated ``rules_python`` to 0.10.2
- **WORKSPACE update needed**: You now need to load ``grpc_extra_deps`` in your WORKSPACE file. See
  the example workspaces for the new template

Rust
****

- Updated ``rules_rust`` to 0.9.0

Scala
*****

- Updated ``rules_scala`` to latest
- Updated ``ScalaPB`` to 0.11.10

Swift
*****

- Updated ``rules_swift`` to 1.1.0


4.1.1
-----

Python
******

- Ensured Python dependencies are correctly updated


4.1.0
-----

The 4.1.0 is mostly an incremental update of dependencies. However, users of the Go and grpc-gateway
rules should see the note below about a change in WORKSPACE order required to avoid resolving very
old versions of dependencies via Gazelle.

General
*******

- Updated protobuf to 3.19.1
- Updated grpc to 1.42.0

C#/F#
*****

- Updated gRPC to 2.42.0
- Updated ``rules_dotnet`` to latest

Go
**

- Updated ``rules_go`` to 0.29.0
- Updated ``gazelle`` to 0.24.0. Note that Gazelle has added multiple dependencies in 0.24.0 that
  conflict with our dependencies and are at quite old versions. If you get an error about
  ``SupportPackageIsVersion7``, you must swap the order you run ``gazelle_dependencies()`` in your
  WORKSPACE to be after ``rules_proto_grpc_go_repos``. See
  `this issue <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/160>`__ for further
  details
- Updated ``com_github_envoyproxy_protoc_gen_validate`` to 0.6.2

grpc-gateway
************

- See above note about Gazelle

gRPC-Web
********

- Updated ``grpc-web`` to 1.3.0

JavaScript
**********

- Updated ``rules_nodejs`` to 4.4.6
- Updated ``@grpc/grpc-js`` to 1.4.4

Python
******

- Updated ``rules_python`` to 0.5.0

Ruby
****

- Updated ``rules_ruby`` to 0.6.0

Rust
****

- Updated ``rules_rust`` to latest. Note that new ``rules_rust`` commits have moved their
  rules definitions from ``/rust/rust.bzl`` to ``/rust/defs.bzl``, which is now required to be
  followed by these rules. No backwards compatibility is possible here as the original path has been
  removed

Scala
*****

- Updated ``rules_scala`` to latest
- Updated ``ScalaPB`` to 0.11.6

Swift
*****

- Updated ``rules_swift`` to 0.24.0
- Updated ``grpc-swift`` to 1.6.0


4.0.1
-----

General
*******

- Fixed plugin label specific values in ``options`` attr being ignored


4.0.0
-----

The 4.0.0 release brings a number of key improvements to tidy up rules_proto_grpc, along with
updates to all of the main dependencies. For most users, 4.0.0 will be a drop-in replacement to
the 3.x.x releases and the updates for each language are shown below. Should you have any issues
with the new release, please open a new
`issue <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/new>`__ or
`discussion <https://github.com/rules-proto-grpc/rules_proto_grpc/discussions/new>`__.

The following changes are considered 'breaking', requiring the step to the 4.x.x release cycle:

- The transitive aspect-based compilation mode using the ``deps`` attribute is now completely
  removed. This mode was deprecated in 3.0.0 and all use of the transitive mode will have shown a
  warning. If all of your uses of rules_proto_grpc use the ``protos`` attribute, 4.0.0 will be no
  different from 3.x.x. See
  `here <https://rules-proto-grpc.com/en/latest/transitivity.html>`__ for further details.
  If you have written your own rules for a custom plugin, please see the updated and simplified rule
  template at :ref:`sec_custom_plugins`.

- The ``//nodejs`` aliases for the ``//js`` rules have been removed. Again, these were deprecated in
  the 3.x.x cycle and printed a warning when used. If you are still using these aliases, you can
  simply change your imports to use the ``//js`` prefixed rules.

- The Rust rules have switched gRPC implementation to `grpc <https://crates.io/crates/grpc>`__.
  In 3.x.x, we used `grpc-rs`/`grpcio`, which wraps the C/C++ implementation of gRPC directly.
  However, the wrapping process was extremely error prone, with updates of either Rust rules or gRPC
  causing linker failures and significant maintenance burden. Should you still need `grpcio` crate
  support, the 3.1.1 release continues to work but may have issues with newer gRPC versions. The
  replacement `grpc` crate is self-described as 'not suitable for production use' but is more
  readily supportable by these rules in the short term. In the longer term, support for
  `prost <https://github.com/tokio-rs/prost>`__ and `tonic <https://github.com/hyperium/tonic>`__
  is also on the roadmap, but is
  `waiting for protoc plugins <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/143>`__
  to be available.

- When using JavaScript library rules, the require path for generated files no longer includes the
  ``<target_name>_pb`` path segment by default. For the previous behaviour, set
  ``legacy_path = True`` on the library.
  `#107 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/107>`__

General
*******

- Updated protobuf to 3.18.0
- Updated grpc to 1.40.0
- Updated ``rules_proto`` to 4.0.0
- Documentation has moved to `rules-proto-grpc.com <https://rules-proto-grpc.com>`__. Existing links
  to the old location will continue to work
- Transitive aspect-based compilation has been removed
- The ``output_files`` attribute of ``ProtoCompileInfo`` has changed from a dict of depsets to a
  single depset. This is generally an internal implementation detail, so is unlikely to affect any
  rule users.

C
*

- Updated ``upb`` to latest

C#/F#
*****

- Added F# support. `#127 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/127>`__
- Updated gRPC to 2.40.0

D
*

- Updated ``rules_d`` to latest

Doc
***

- Updated ``protoc-gen-doc`` to 1.5.0
- Added ``doc_template_compile`` to generate output using a custom Go template file.

Go
**

- Updated ``rules_go`` to v0.28.0
- Added validator rules using
  `protoc-gen-validate <https://github.com/envoyproxy/protoc-gen-validate>`__.
  `#16 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/16>`__

grpc-gateway
************

- Updated ``grpc-gateway`` to 2.6.0

Java
****

- Updated ``grpc-java`` to 1.40.1

JavaScript
**********

- **Breaking change**: The require path for generated files no longer includes the
  ``<target_name>_pb`` path segment by default. For the previous behaviour, set
  ``legacy_path = True`` on the library.
  `#107 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/107>`__
- Added ``package_name`` attribute to library rules, which allows customising the package name of
  the generated library. By default if unspecified, the target name will continue to be used as
  in previous versions.
- Updated ``rules_nodejs`` to 4.2.0
- Updated ``@grpc/grpc-js`` to 1.3.7
- Updated ``grpc-tools`` to 1.11.2
- Updated ``ts-protoc-gen`` to 0.15.0

Python
******

- Updated ``rules_python`` to 0.4.0
- Updated ``six`` to 1.16.0

Ruby
****

- Updated ``rules_ruby`` to 0.5.2
- **WORKSPACE update needed**: The ``ruby_bundle`` call in your workspace needs an extra ``include``
  attribute for grpc to work as expected. Please see the Ruby examples

Rust
****

- Updated ``rules_rust`` to latest
- **Breaking change**: Replaced ``grpcio`` with ``grpc``. Please see above description for
  full details on why ``grpcio`` is no longer supportable and the long term aim to support prost and
  tonic
- Updated ``protobuf`` and ``protobuf-codegen`` to 2.25.1

Scala
*****

- Updated ``rules_scala`` to latest
- Updated ``ScalaPB`` to 0.11.5
- **WORKSPACE update needed**: Dependencies are now fetched with ``maven_install``. You will need to
  update your WORKSPACE to match the current example.

Swift
*****

- Updated ``rules_swift`` to 0.23.0
- Updated ``grpc-swift`` to 1.4.1
- Updated ``swift-log`` to 1.4.2
- Updated ``swift-nio`` to 2.32.3
- Updated ``swift-nio-extra`` to 1.10.2
- Updated ``swift-nio-http2`` to 1.18.3
- Updated ``swift-nio-ssl`` to 2.15.1
- Updated ``swift-nio-transport-services`` to 1.11.3

TypeScript
**********

- The default mode for TypeScript gRPC compilation has changed to ``grpc-js``. This means imports
  should now use ``@grpc/grpc-js`` instead of ``grpc``
  `#134 <https://github.com/rules-proto-grpc/rules_proto_grpc/pull/134>`__


3.1.1
-----

Improved documentation is now available at https://rules-proto-grpc.aliddell.com


3.1.0
-----

This update mostly brings fixes to the JavaScript rules, along with new rules for generating
Markdown, JSON, HTML or DocBook documentation from .proto files using
`protoc-gen-doc <https://github.com/pseudomuto/protoc-gen-doc>`__. Additionally, new
``buf_proto_lint`` and ``buf_proto_breaking`` rules have been added to support linting .proto files
and checking for breaking changes using `Buf <https://buf.build>`__.

General
*******

- Updated protobuf to 3.15.3

Buf
***

- Added linting and breaking change detection rules using `Buf <https://buf.build>`__

Doc
***

- Added documentation rules to generate Markdown, JSON, HTML or DocBook files using
  `protoc-gen-doc <https://github.com/pseudomuto/protoc-gen-doc>`__

grpc-gateway
************

- Updated grpc-gateway to 2.3.0
- Fixed issue with mixing .proto files that do and do not contain services
  `#72 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/72>`__

JavaScript
**********

- Updated ``rules_nodejs`` to 3.2.1
- **WORKSPACE update needed**: The dependencies for JavaScript rules must now be loaded into your
  local ``package.json``, which defaults to the name ``@npm``. The ``yarn_install`` with name
  ``js_modules`` in your WORKSPACE can now also be removed
- Updated ``@grpc/grpc-js`` to 1.2.8
- Fixed missing ``DeclarationInfo`` when using the ``js_grpc_node_library`` or
  ``js_grpc_web_library`` rules
  `#113 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/113>`__
- Added a TypeScript test workspace

Objective-C
***********

- Added the ``objc_grpc_library`` experimental rule

Rust
****

- Updated ``rules_rust`` to latest
- Updated ``grpcio`` to 0.8.0
- Updated ``protobuf`` to 2.22.0


3.0.0
-----

This update brings some major improvements to rules_proto_grpc and solves many of the longstanding
issues that have been present. However, in doing so there have been some changes that make a major
version increment necessary and may require updates to your build files. The updates for each
language are explained below and should you have any issues, please open a new
`issue <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/new>`__ or
`discussion <https://github.com/rules-proto-grpc/rules_proto_grpc/discussions/new>`__.

The most substantial change is that compilation of .proto files into language specific files is no
longer transitive. This means that only the direct dependencies of a ``lang_proto_library`` will be
present within the generated library, rather than every transitive proto message. The justification
for this is below, but if you're just interested in the changes, you can skip down to the next
heading.

In previous versions of rules_proto_grpc, the compilation aspect would compile and aggregate all
dependent .proto files from any top level target. In hindsight, this was not the correct behaviour
and led to many bugs, since you may end up creating a library that contains compiled proto files
from a third party, where you should instead be depending on a proper library for that third party's
protos.

Even in a single repo, this may have meant multiple copies of a single compiled proto file being
present in a target, if it is depended on via multiple routes. For some languages, such as C++, this
breaks the 'one definition rule' and produces compilation failures or runtime bugs. For other
languages, such as Python, this just meant unnecessary duplicate files in the output binaries.

Therefore, in this release of rules_proto_grpc, there is now a recommedned option to bundle only the
direct proto dependencies into  the libraries, without including the compiled transitive proto
files. This is done by replacing the ``deps`` attr on ``lang_{proto|grpc}_{compile|library}`` with
the ``protos`` attr. Since this would be a substantial breaking change to drop at once on a large
project, the new behaviour is opt-in in 3.0.0 and the old method continues to work throughout the
3.x.x release cycle. Rules using the previous deps attr will have a warning written to console to
signify that your library may be bundling more than expect and should switch attr.

As an additional benefit of this change, we can now support passing arbitrary per-target rules to
protoc through the new ``options`` attr of the rules, which was a much sought after change that was
impossible in the aspect based compilation.

Switching to non-transitive compilation
***************************************

In short, replace ``deps`` with ``protos`` on your targets:

.. code-block:: python

   # Old
   python_grpc_library(
       name = "routeguide",
       deps = ["//example/proto:routeguide_proto"],
   )

   # New
   python_grpc_library(
       name = "routeguide",
       protos = ["//example/proto:routeguide_proto"],
   )

In applying the above change, you may discover that you were inheriting dependencies transitively
and that your builds now fail. In such cases, you should add a
``lang_{proto|grpc}_{compile|library}`` target for those proto files and depend on it explicitly
from the relevant top level binaries/libraries.

General Changes
***************

- Updated protobuf to 3.15.1
- Updated gRPC to 1.35.0
- All rules have new per-target ``options`` and ``extra_protoc_args`` attributes to control options
  to protoc
  `#54 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/54>`__
  `#68 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/68>`__
  `#105 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/105>`__
- Updated ``rules_proto`` to latest head
- ``aspect.bzl`` and ``plugin.bzl`` have merged to a single top level ``defs.bzl``
- The minimum supported Bazel version is 3.0.0. Some language specific rules may require 4.0.0

Android
*******

- **WORKSPACE update needed**: The WORKSPACE imports necessary for Android rules have been updated
  due to upstream changes in ``grpc-java``. Please see the examples for the latest WORKSPACE
  template for the Android rules

C
*

- Added experimental rules for C using upb
  `#20 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/20>`__

C++
***

- Non-transitive mode resolves issue where the same proto may be defined more than once
  `#25 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/25>`__
- Header and source files are now correctly passed to the underlying ``cc_library`` rule
  `#40 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/40>`__

Closure
*******

- Closure rules have been removed. In practice these have been superceded by the Javascript rules,
  but if you are an active user of these rules please open a discussion.

C#
**

- Updated ``rules_dotnet`` to 0.0.7. Note that the new versions of ``rules_dotnet`` drop support for
  .Net Framework and Mono and require use of alternate platforms. Please see the examples for the
  latest WORKSPACE template for the C# rules
- Updated ``Grpc`` to 2.35.0

D
*

- Updated ``rules_d`` to latest

Go
**

- Updated ``rules_go`` to 0.25.1
- **WORKSPACE update needed**: It is now necessary to specify ``version`` to
  ``go_register_toolchains``
- The plugin used for compiling .proto files for Go has switched to the new
  google.golang.org/protobuf `#85 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/85>`__
- Updated ``gazelle`` to 0.22.3
- Updated ``org_golang_x_net`` to v0.0.0-20210129194117-4acb7895a057
- Updated ``org_golang_x_text`` to 0.3.5
- Well-known types are now depended on by default
- Removed support for GoGo rules

grpc-gateway
************

- Updated ``grpc-gateway`` to 2.2.0
- The ``gateway_swagger_compile`` rule has been replaced with ``gateway_openapiv2_compile``
  `#93 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/93>`__
- The grpc-gateway rules have move to repo top level, meaning they are no longer under the
  ``github.com/...`` prefix. To update your use of these rules find and replace
  ``@rules_proto_grpc//github.com/grpc-ecosystem/grpc-gateway`` with
  ``@rules_proto_grpc//grpc-gateway``

gRPC-Web
********

- The gRPC-Web rules have moved into ``//js``
- Text mode generation is now supported
  `#59 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/59>`__

Java
****

- **WORKSPACE update needed**: The WORKSPACE imports necessary for Java rules have been updated due
  to upstream changes in ``grpc-java``. Please see the examples for the latest WORKSPACE template
  for the Java rules

NodeJS/JavaScript
*****************

- The JavaScript rules have moved from ``@rules_proto_grpc//nodejs`` to ``@rules_proto_grpc//js``,
  but the old rules are still aliased to ease transition
- Updated ``rules_nodejs`` to 3.1.0
- Updated ``@grpc/grpc-js`` to 1.2.6
- Added typescript generation to JS rules

Objective-C
***********

- Added ``copt`` argument pass-through for Obj-C library rules.
- Header and source files are now correctly passed to the underlying ``cc_library`` rule
  `#40 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/40>`__

Python
******

- Updated ``rules_python`` to latest
- **WORKSPACE update needed**: ``py_repositories`` from ``rules_python`` is no longer required

Ruby
****

- The Ruby rules have migrated from ``yugui/rules_ruby`` to ``bazelruby/rules_ruby``
- Changed ``rules_proto_grpc_gems`` to ``rules_proto_grpc_bundle``
- **WORKSPACE update needed**: The above changes requiresupdates to your WORKSPACE, please see the
  examples for the latest WORKSPACE template for the Ruby rules
- **Open issue**: The `grpc` gem may not be loadable in generated Ruby libraries, please see
  `this issue <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/65>`__

Rust
****

- **WORKSPACE update needed**: The upstream repo ``io_bazel_rules_rust`` has been renamed to
  ``rules_rust``. The ``rust_workspace`` rule is also no longer required
- Updated ``rules_rust`` to latest
- Updated ``grpcio`` to 0.7.1
- Updated ``protobuf`` to 2.20.0

Scala
*****

- Update ``rules_scala`` to latest
  `#108 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/108>`__
- **WORKSPACE update needed**: The ``scala_config`` rule from ``rules_scala`` is now required in
  your WORKSPACE

Swift
*****

- Updated ``rules_swift`` to 0.18.0
- Updated ``grpc-swift`` to 1.0.0
- Visibility of generated types is now configurable with ``options``
  `#111 <https://github.com/rules-proto-grpc/rules_proto_grpc/issues/111>`__

Thanks
******

Thanks to everyone who has contributed issues and patches for this release.


2.0.0
-----

General
*******

- Updated ``protobuf`` to 3.13.0
- Updated ``grpc`` to 1.32.0
- **WORKSPACE update needed**: These rules now depend on ``rules_proto``, which must be added to
  your WORKSPACE file
- Dropped support for the deprecated ``transitivity`` attribute on ``proto_plugin``. The
  ``exclusions`` attribute is the supported way of achieving this
- The ``output_dirs`` attribute of ``ProtoCompileInfo`` is now a depset, meaning directories will be
  deduplicated
- Removed the ``deps.bzl`` files that have been deprecated since version 1.0.0
- Tags are now propagated correctly on library rules

Android
*******

- **WORKSPACE update needed**: The Guava dependency is no longer needed

C#
**

- Updated ``rules_dotnet`` to latest master
- Updated ``Google.Protobuf`` to 3.13.0
- Updated ``Grpc`` to 2.32.0
- **WORKSPACE update needed**: There have been substantial changes to the required WORKSPACE rules
  for C#. Please see the C# language page

Closure
*******

- Updated ``rules_closure`` to 0.11.0

D
*

- Updated ``rules_d`` to latest master
- Updated ``protobuf-d`` to 0.6.2

grpc-gateway
************

- Updated ``grpc-gateway`` to 1.15.0

gRPC Web
********

- Updated gRPC Web to 1.2.1

Go
**

- Updated ``rules_go`` to 0.24.3
- Updated ``bazel-gazelle`` to 0.21.1
- Updated ``org_golang_x_net`` to v0.0.0-20200930145003-4acb6c075d10
- Updated ``org_golang_x_text`` to 0.3.3

Java
****
- **WORKSPACE update needed**: The Guava dependency is no longer needed

NodeJS
******

- Updated ``rules_nodejs`` to 2.2.0
- **WORKSPACE update needed**: The ``defs.bzl`` file in ``rules_nodejs`` has moved to ``index.bzl``
- **WORKSPACE update needed**: Running ``yarn_install()`` is needed in more cases
- **WORKSPACE update needed**: Running ``grpc_deps()`` is no longer necessary for just the NodeJS
  rules
- Moved from ``grpc`` to ``@grpc/grpc-js`` package
- Library rules have been enabled and now return ``js_library`` rather than ``npm_package``

Python
******

- Dropped Python 2 support
- Updated ``rules_python`` to latest master
- Updated ``grpclib`` to 0.4.1
- Moved to using ``grpcio`` library directly from the local ``grpc`` repository.
- Pinned dependency versions in requirements.txt using pip-compile
- **WORKSPACE update needed**: The method for loading Pip dependencies has changed. Please see the
  Python language page.
- **WORKSPACE update needed**: Using the Pip dependencies is now only necessary if you are using the
  ``grpclib`` rules

Rust
****

- Updated ``rules_rust`` to latest master
- Updated ``protobuf`` crate to 2.17.0
- Updated ``grpcio`` crate to 0.6.0
- **WORKSPACE update needed**: The setup for ``rules_rust`` has changed in the newer version. Please
  see the Rust language page.
- **WORKSPACE update needed**: The ``grpc_deps()`` rule is now needed for Rust

Scala
*****

- Updated ``rules_scala`` to latest master
- ``ScalaPB`` is now pulled from ``rules_scala``, which uses 0.9.7
- **WORKSPACE update needed**: The ``scala_proto_repositories()`` rule is now needed

Swift
*****

- Updated ``rules_swift`` to 0.15.0
- Updated ``grpc-swift`` to 0.11.0
- Moved the Swift library rules to be internal to this repo


1.0.2
-----

Android / Closure / Java / Scala
********************************

- Fixed loading of ``com_google_errorprone_error_prone_annotations``
- Replaced Maven HTTP URLs with HTTPS URLs
- Updated grpc-java, rules_closure and rules_scala to include Maven HTTPS fix


1.0.1
-----

General
*******

- Fix support for plugins that use ``output_directory`` and produce no output files: #39 
- Misc typo fixes and tidying


1.0.0
-----

General
*******

- Bazel 1.0+ is now supported
- The ``rules_proto_grpc_repos()`` WORKSPACE rule has been added and is recommended to be used
- Protobuf has been updated to 3.11.0
- gRPC has been updated to 1.25.0
- All other dependencies have been updated where available
- The Bazel version is now checked for compatibility
- Added more test workspaces
- Removed tests that use ``proto_source_root``
- Added fix for duplicate proto files when using ``import_prefix``

Closure
*******

- The required WORKSPACE rules has been updated for all Closure-based rules, please check the
  documentation for the current recommended set

Go / GoGo / grpc-gateway
************************

- The required WORKSPACE rules has been updated for all Go-based rules, please check the
  documentation for the current recommended set

gRPC.js
*******

- Support for gRPC.js has been removed

Python
******

- The way dependencies are pulled in has changed from using ``rules_pip`` to the standard
  ``rules_python``. Please check the documentation for the new WORKSPACE rules required and remove
  the old ones

Scala
*****

- Scala gRPC rules are currently not working fully. Due to delays in publishing support for Bazel
  1.0, this support has been pushed back to 1.1.0
- The required WORKSPACE rules has been updated for all Scala rules, please check the documentation
  for the current recommended set


0.2.0
-----

General
*******

- Tests generated by the routeguide test matrix now correctly us the client/server executables

Ruby
****

- Well-known proto files are excluded from generation in the Ruby plugins
- The naming of the Ruby gems workspace has changed to remove the 'routeguide' prefix
- Ruby client/server is now included in the non-manual test matrix


0.1.0
-----

Initial release of ``rules_proto_grpc``. For changes from predecessor ``rules_proto``, please see
`MIGRATION.md <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/0.1.0/docs/MIGRATION.md>`__
