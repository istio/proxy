<div align="center">
    <img width="200" height="200" src="https://raw.githubusercontent.com/rules-proto-grpc/rules_proto_grpc/master/docs/_static/logo.svg">
    <h1>Protobuf and gRPC rules for <a href="https://bazel.build">Bazel</a></h1>
</div>

<div align="center">
    <a href="https://bazel.build">Bazel</a> rules for building <a href="https://developers.google.com/protocol-buffers">Protobuf</a> and <a href="https://grpc.io/">gRPC</a> code and libraries from <a href="https://docs.bazel.build/versions/master/be/protocol-buffer.html#proto_library">proto_library</a> targets<br><br>
    <a href="https://github.com/rules-proto-grpc/rules_proto_grpc/releases"><img src="https://img.shields.io/github/v/tag/rules-proto-grpc/rules_proto_grpc?label=release&sort=semver&color=38a3a5"></a>
    <a href="https://buildkite.com/bazel/rules-proto-grpc-rules-proto-grpc"><img src="https://badge.buildkite.com/a0c88e60f21c85a8bb53a8c73175aebd64f50a0d4bacbdb038.svg?branch=master"></a>
    <a href="https://github.com/rules-proto-grpc/rules_proto_grpc/actions"><img src="https://github.com/rules-proto-grpc/rules_proto_grpc/workflows/CI/badge.svg"></a>
    <a href="https://bazelbuild.slack.com/archives/CKU1D04RM"><img src="https://img.shields.io/badge/bazelbuild-%23proto-38a3a5?logo=slack"></a>
</div>


## Announcements 📣

#### 2023/12/14 - Version 4.6.0

[Version 4.6.0 has been released](https://github.com/rules-proto-grpc/rules_proto_grpc/releases/tag/4.6.0),
which contains a few bug fixes for Bazel 7 support. **Note that this is likely to be the last
WORKSPACE supporting release of rules_proto_grpc**, as new bzlmod supporting rules are introduced
in the next major release

#### 2023/09/12 - Version 4.5.0

[Version 4.5.0 has been released](https://github.com/rules-proto-grpc/rules_proto_grpc/releases/tag/4.5.0),
which contains a number of version updates, bug fixes and usability improvements over 4.4.0.
Additionally, the Rust rules contain a major change of underlying gRPC and Protobuf library; the
rules now use Tonic and Prost respectively


## Usage

Full documentation for the current and previous versions [can be found here](https://rules-proto-grpc.com)

- [Overview](https://rules-proto-grpc.com/en/latest/)
- [Installation](https://rules-proto-grpc.com/en/latest/#installation)
- [Example Usage](https://rules-proto-grpc.com/en/latest/example.html)
- [Custom Plugins](https://rules-proto-grpc.com/en/latest/custom_plugins.html)
- [Changelog](https://rules-proto-grpc.com/en/latest/changelog.html)


## Rules

| Language | Rule | Description
| ---: | :--- | :--- |
| [Android](https://rules-proto-grpc.com/en/latest/lang/android.html) | [android_proto_compile](https://rules-proto-grpc.com/en/latest/lang/android.html#android-proto-compile) | Generates an Android protobuf ``.jar`` file ([example](/example/android/android_proto_compile)) |
| [Android](https://rules-proto-grpc.com/en/latest/lang/android.html) | [android_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/android.html#android-grpc-compile) | Generates Android protobuf and gRPC ``.jar`` files ([example](/example/android/android_grpc_compile)) |
| [Android](https://rules-proto-grpc.com/en/latest/lang/android.html) | [android_proto_library](https://rules-proto-grpc.com/en/latest/lang/android.html#android-proto-library) | Generates an Android protobuf library using ``android_library`` from ``rules_android`` ([example](/example/android/android_proto_library)) |
| [Android](https://rules-proto-grpc.com/en/latest/lang/android.html) | [android_grpc_library](https://rules-proto-grpc.com/en/latest/lang/android.html#android-grpc-library) | Generates Android protobuf and gRPC library using ``android_library`` from ``rules_android`` ([example](/example/android/android_grpc_library)) |
| [Buf](https://rules-proto-grpc.com/en/latest/lang/buf.html) | [buf_proto_breaking_test](https://rules-proto-grpc.com/en/latest/lang/buf.html#buf-proto-breaking-test) | Checks .proto files for breaking changes ([example](/example/buf/buf_proto_breaking_test)) |
| [Buf](https://rules-proto-grpc.com/en/latest/lang/buf.html) | [buf_proto_lint_test](https://rules-proto-grpc.com/en/latest/lang/buf.html#buf-proto-lint-test) | Lints .proto files ([example](/example/buf/buf_proto_lint_test)) |
| [C](https://rules-proto-grpc.com/en/latest/lang/c.html) | [c_proto_compile](https://rules-proto-grpc.com/en/latest/lang/c.html#c-proto-compile) | Generates C protobuf ``.h`` & ``.c`` files ([example](/example/c/c_proto_compile)) |
| [C](https://rules-proto-grpc.com/en/latest/lang/c.html) | [c_proto_library](https://rules-proto-grpc.com/en/latest/lang/c.html#c-proto-library) | Generates a C protobuf library using ``cc_library``, with dependencies linked ([example](/example/c/c_proto_library)) |
| [C++](https://rules-proto-grpc.com/en/latest/lang/cpp.html) | [cpp_proto_compile](https://rules-proto-grpc.com/en/latest/lang/cpp.html#cpp-proto-compile) | Generates C++ protobuf ``.h`` & ``.cc`` files ([example](/example/cpp/cpp_proto_compile)) |
| [C++](https://rules-proto-grpc.com/en/latest/lang/cpp.html) | [cpp_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/cpp.html#cpp-grpc-compile) | Generates C++ protobuf and gRPC ``.h`` & ``.cc`` files ([example](/example/cpp/cpp_grpc_compile)) |
| [C++](https://rules-proto-grpc.com/en/latest/lang/cpp.html) | [cpp_proto_library](https://rules-proto-grpc.com/en/latest/lang/cpp.html#cpp-proto-library) | Generates a C++ protobuf library using ``cc_library``, with dependencies linked ([example](/example/cpp/cpp_proto_library)) |
| [C++](https://rules-proto-grpc.com/en/latest/lang/cpp.html) | [cpp_grpc_library](https://rules-proto-grpc.com/en/latest/lang/cpp.html#cpp-grpc-library) | Generates a C++ protobuf and gRPC library using ``cc_library``, with dependencies linked ([example](/example/cpp/cpp_grpc_library)) |
| [C#](https://rules-proto-grpc.com/en/latest/lang/csharp.html) | [csharp_proto_compile](https://rules-proto-grpc.com/en/latest/lang/csharp.html#csharp-proto-compile) | Generates C# protobuf ``.cs`` files ([example](/example/csharp/csharp_proto_compile)) |
| [C#](https://rules-proto-grpc.com/en/latest/lang/csharp.html) | [csharp_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/csharp.html#csharp-grpc-compile) | Generates C# protobuf and gRPC ``.cs`` files ([example](/example/csharp/csharp_grpc_compile)) |
| [C#](https://rules-proto-grpc.com/en/latest/lang/csharp.html) | [csharp_proto_library](https://rules-proto-grpc.com/en/latest/lang/csharp.html#csharp-proto-library) | Generates a C# protobuf library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll`` ([example](/example/csharp/csharp_proto_library)) |
| [C#](https://rules-proto-grpc.com/en/latest/lang/csharp.html) | [csharp_grpc_library](https://rules-proto-grpc.com/en/latest/lang/csharp.html#csharp-grpc-library) | Generates a C# protobuf and gRPC library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll`` ([example](/example/csharp/csharp_grpc_library)) |
| [D](https://rules-proto-grpc.com/en/latest/lang/d.html) | [d_proto_compile](https://rules-proto-grpc.com/en/latest/lang/d.html#d-proto-compile) | Generates D protobuf ``.d`` files ([example](/example/d/d_proto_compile)) |
| [D](https://rules-proto-grpc.com/en/latest/lang/d.html) | [d_proto_library](https://rules-proto-grpc.com/en/latest/lang/d.html#d-proto-library) | Generates a D protobuf library using ``d_library`` from ``rules_d`` ([example](/example/d/d_proto_library)) |
| [Documentation](https://rules-proto-grpc.com/en/latest/lang/doc.html) | [doc_docbook_compile](https://rules-proto-grpc.com/en/latest/lang/doc.html#doc-docbook-compile) | Generates DocBook ``.xml`` documentation file ([example](/example/doc/doc_docbook_compile)) |
| [Documentation](https://rules-proto-grpc.com/en/latest/lang/doc.html) | [doc_html_compile](https://rules-proto-grpc.com/en/latest/lang/doc.html#doc-html-compile) | Generates ``.html`` documentation file ([example](/example/doc/doc_html_compile)) |
| [Documentation](https://rules-proto-grpc.com/en/latest/lang/doc.html) | [doc_json_compile](https://rules-proto-grpc.com/en/latest/lang/doc.html#doc-json-compile) | Generates ``.json`` documentation file ([example](/example/doc/doc_json_compile)) |
| [Documentation](https://rules-proto-grpc.com/en/latest/lang/doc.html) | [doc_markdown_compile](https://rules-proto-grpc.com/en/latest/lang/doc.html#doc-markdown-compile) | Generates Markdown ``.md`` documentation file ([example](/example/doc/doc_markdown_compile)) |
| [Documentation](https://rules-proto-grpc.com/en/latest/lang/doc.html) | [doc_template_compile](https://rules-proto-grpc.com/en/latest/lang/doc.html#doc-template-compile) | Generates documentation file using Go template file ([example](/example/doc/doc_template_compile)) |
| [F#](https://rules-proto-grpc.com/en/latest/lang/fsharp.html) | [fsharp_proto_compile](https://rules-proto-grpc.com/en/latest/lang/fsharp.html#fsharp-proto-compile) | Generates F# protobuf ``.fs`` files ([example](/example/fsharp/fsharp_proto_compile)) |
| [F#](https://rules-proto-grpc.com/en/latest/lang/fsharp.html) | [fsharp_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/fsharp.html#fsharp-grpc-compile) | Generates F# protobuf and gRPC ``.fs`` files ([example](/example/fsharp/fsharp_grpc_compile)) |
| [F#](https://rules-proto-grpc.com/en/latest/lang/fsharp.html) | [fsharp_proto_library](https://rules-proto-grpc.com/en/latest/lang/fsharp.html#fsharp-proto-library) | Generates a F# protobuf library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll`` ([example](/example/fsharp/fsharp_proto_library)) |
| [F#](https://rules-proto-grpc.com/en/latest/lang/fsharp.html) | [fsharp_grpc_library](https://rules-proto-grpc.com/en/latest/lang/fsharp.html#fsharp-grpc-library) | Generates a F# protobuf and gRPC library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll`` ([example](/example/fsharp/fsharp_grpc_library)) |
| [Go](https://rules-proto-grpc.com/en/latest/lang/go.html) | [go_proto_compile](https://rules-proto-grpc.com/en/latest/lang/go.html#go-proto-compile) | Generates Go protobuf ``.go`` files ([example](/example/go/go_proto_compile)) |
| [Go](https://rules-proto-grpc.com/en/latest/lang/go.html) | [go_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/go.html#go-grpc-compile) | Generates Go protobuf and gRPC ``.go`` files ([example](/example/go/go_grpc_compile)) |
| [Go](https://rules-proto-grpc.com/en/latest/lang/go.html) | [go_validate_compile](https://rules-proto-grpc.com/en/latest/lang/go.html#go-validate-compile) | Generates Go protobuf and gRPC validation ``.go`` files ([example](/example/go/go_validate_compile)) |
| [Go](https://rules-proto-grpc.com/en/latest/lang/go.html) | [go_proto_library](https://rules-proto-grpc.com/en/latest/lang/go.html#go-proto-library) | Generates a Go protobuf library using ``go_library`` from ``rules_go`` ([example](/example/go/go_proto_library)) |
| [Go](https://rules-proto-grpc.com/en/latest/lang/go.html) | [go_grpc_library](https://rules-proto-grpc.com/en/latest/lang/go.html#go-grpc-library) | Generates a Go protobuf and gRPC library using ``go_library`` from ``rules_go`` ([example](/example/go/go_grpc_library)) |
| [Go](https://rules-proto-grpc.com/en/latest/lang/go.html) | [go_validate_library](https://rules-proto-grpc.com/en/latest/lang/go.html#go-validate-library) | Generates a Go protobuf and gRPC validation library using ``go_library`` from ``rules_go`` ([example](/example/go/go_validate_library)) |
| [grpc-gateway](https://rules-proto-grpc.com/en/latest/lang/grpc-gateway.html) | [gateway_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/grpc-gateway.html#gateway-grpc-compile) | Generates grpc-gateway ``.go`` files ([example](/example/grpc-gateway/gateway_grpc_compile)) |
| [grpc-gateway](https://rules-proto-grpc.com/en/latest/lang/grpc-gateway.html) | [gateway_openapiv2_compile](https://rules-proto-grpc.com/en/latest/lang/grpc-gateway.html#gateway-openapiv2-compile) | Generates grpc-gateway OpenAPI v2 ``.json`` files ([example](/example/grpc-gateway/gateway_openapiv2_compile)) |
| [grpc-gateway](https://rules-proto-grpc.com/en/latest/lang/grpc-gateway.html) | [gateway_grpc_library](https://rules-proto-grpc.com/en/latest/lang/grpc-gateway.html#gateway-grpc-library) | Generates grpc-gateway library files ([example](/example/grpc-gateway/gateway_grpc_library)) |
| [Java](https://rules-proto-grpc.com/en/latest/lang/java.html) | [java_proto_compile](https://rules-proto-grpc.com/en/latest/lang/java.html#java-proto-compile) | Generates a Java protobuf srcjar file ([example](/example/java/java_proto_compile)) |
| [Java](https://rules-proto-grpc.com/en/latest/lang/java.html) | [java_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/java.html#java-grpc-compile) | Generates a Java protobuf and gRPC srcjar file ([example](/example/java/java_grpc_compile)) |
| [Java](https://rules-proto-grpc.com/en/latest/lang/java.html) | [java_proto_library](https://rules-proto-grpc.com/en/latest/lang/java.html#java-proto-library) | Generates a Java protobuf library using ``java_library`` ([example](/example/java/java_proto_library)) |
| [Java](https://rules-proto-grpc.com/en/latest/lang/java.html) | [java_grpc_library](https://rules-proto-grpc.com/en/latest/lang/java.html#java-grpc-library) | Generates a Java protobuf and gRPC library using ``java_library`` ([example](/example/java/java_grpc_library)) |
| [JavaScript](https://rules-proto-grpc.com/en/latest/lang/js.html) | [js_proto_compile](https://rules-proto-grpc.com/en/latest/lang/js.html#js-proto-compile) | Generates JavaScript protobuf ``.js`` and ``.d.ts`` files ([example](/example/js/js_proto_compile)) |
| [JavaScript](https://rules-proto-grpc.com/en/latest/lang/js.html) | [js_grpc_node_compile](https://rules-proto-grpc.com/en/latest/lang/js.html#js-grpc-node-compile) | Generates JavaScript protobuf and gRPC-node ``.js`` and ``.d.ts`` files ([example](/example/js/js_grpc_node_compile)) |
| [JavaScript](https://rules-proto-grpc.com/en/latest/lang/js.html) | [js_grpc_web_compile](https://rules-proto-grpc.com/en/latest/lang/js.html#js-grpc-web-compile) | Generates JavaScript protobuf and gRPC-Web ``.js`` and ``.d.ts`` files ([example](/example/js/js_grpc_web_compile)) |
| [JavaScript](https://rules-proto-grpc.com/en/latest/lang/js.html) | [js_proto_library](https://rules-proto-grpc.com/en/latest/lang/js.html#js-proto-library) | Generates a JavaScript protobuf library using ``js_library`` from ``rules_nodejs`` ([example](/example/js/js_proto_library)) |
| [JavaScript](https://rules-proto-grpc.com/en/latest/lang/js.html) | [js_grpc_node_library](https://rules-proto-grpc.com/en/latest/lang/js.html#js-grpc-node-library) | Generates a Node.js protobuf + gRPC-node library using ``js_library`` from ``rules_nodejs`` ([example](/example/js/js_grpc_node_library)) |
| [JavaScript](https://rules-proto-grpc.com/en/latest/lang/js.html) | [js_grpc_web_library](https://rules-proto-grpc.com/en/latest/lang/js.html#js-grpc-web-library) | Generates a JavaScript protobuf + gRPC-Web library using ``js_library`` from ``rules_nodejs`` ([example](/example/js/js_grpc_web_library)) |
| [Objective-C](https://rules-proto-grpc.com/en/latest/lang/objc.html) | [objc_proto_compile](https://rules-proto-grpc.com/en/latest/lang/objc.html#objc-proto-compile) | Generates Objective-C protobuf ``.m`` & ``.h`` files ([example](/example/objc/objc_proto_compile)) |
| [Objective-C](https://rules-proto-grpc.com/en/latest/lang/objc.html) | [objc_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/objc.html#objc-grpc-compile) | Generates Objective-C protobuf and gRPC ``.m`` & ``.h`` files ([example](/example/objc/objc_grpc_compile)) |
| [Objective-C](https://rules-proto-grpc.com/en/latest/lang/objc.html) | [objc_proto_library](https://rules-proto-grpc.com/en/latest/lang/objc.html#objc-proto-library) | Generates an Objective-C protobuf library using ``objc_library`` ([example](/example/objc/objc_proto_library)) |
| [Objective-C](https://rules-proto-grpc.com/en/latest/lang/objc.html) | [objc_grpc_library](https://rules-proto-grpc.com/en/latest/lang/objc.html#objc-grpc-library) | Generates an Objective-C protobuf and gRPC library using ``objc_library`` ([example](/example/objc/objc_grpc_library)) |
| [PHP](https://rules-proto-grpc.com/en/latest/lang/php.html) | [php_proto_compile](https://rules-proto-grpc.com/en/latest/lang/php.html#php-proto-compile) | Generates PHP protobuf ``.php`` files ([example](/example/php/php_proto_compile)) |
| [PHP](https://rules-proto-grpc.com/en/latest/lang/php.html) | [php_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/php.html#php-grpc-compile) | Generates PHP protobuf and gRPC ``.php`` files ([example](/example/php/php_grpc_compile)) |
| [Python](https://rules-proto-grpc.com/en/latest/lang/python.html) | [python_proto_compile](https://rules-proto-grpc.com/en/latest/lang/python.html#python-proto-compile) | Generates Python protobuf ``.py`` files ([example](/example/python/python_proto_compile)) |
| [Python](https://rules-proto-grpc.com/en/latest/lang/python.html) | [python_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/python.html#python-grpc-compile) | Generates Python protobuf and gRPC ``.py`` files ([example](/example/python/python_grpc_compile)) |
| [Python](https://rules-proto-grpc.com/en/latest/lang/python.html) | [python_grpclib_compile](https://rules-proto-grpc.com/en/latest/lang/python.html#python-grpclib-compile) | Generates Python protobuf and grpclib ``.py`` files (supports Python 3 only) ([example](/example/python/python_grpclib_compile)) |
| [Python](https://rules-proto-grpc.com/en/latest/lang/python.html) | [python_proto_library](https://rules-proto-grpc.com/en/latest/lang/python.html#python-proto-library) | Generates a Python protobuf library using ``py_library`` from ``rules_python`` ([example](/example/python/python_proto_library)) |
| [Python](https://rules-proto-grpc.com/en/latest/lang/python.html) | [python_grpc_library](https://rules-proto-grpc.com/en/latest/lang/python.html#python-grpc-library) | Generates a Python protobuf and gRPC library using ``py_library`` from ``rules_python`` ([example](/example/python/python_grpc_library)) |
| [Python](https://rules-proto-grpc.com/en/latest/lang/python.html) | [python_grpclib_library](https://rules-proto-grpc.com/en/latest/lang/python.html#python-grpclib-library) | Generates a Python protobuf and grpclib library using ``py_library`` from ``rules_python`` (supports Python 3 only) ([example](/example/python/python_grpclib_library)) |
| [Ruby](https://rules-proto-grpc.com/en/latest/lang/ruby.html) | [ruby_proto_compile](https://rules-proto-grpc.com/en/latest/lang/ruby.html#ruby-proto-compile) | Generates Ruby protobuf ``.rb`` files ([example](/example/ruby/ruby_proto_compile)) |
| [Ruby](https://rules-proto-grpc.com/en/latest/lang/ruby.html) | [ruby_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/ruby.html#ruby-grpc-compile) | Generates Ruby protobuf and gRPC ``.rb`` files ([example](/example/ruby/ruby_grpc_compile)) |
| [Ruby](https://rules-proto-grpc.com/en/latest/lang/ruby.html) | [ruby_proto_library](https://rules-proto-grpc.com/en/latest/lang/ruby.html#ruby-proto-library) | Generates a Ruby protobuf library using ``ruby_library`` from ``rules_ruby`` ([example](/example/ruby/ruby_proto_library)) |
| [Ruby](https://rules-proto-grpc.com/en/latest/lang/ruby.html) | [ruby_grpc_library](https://rules-proto-grpc.com/en/latest/lang/ruby.html#ruby-grpc-library) | Generates a Ruby protobuf and gRPC library using ``ruby_library`` from ``rules_ruby`` ([example](/example/ruby/ruby_grpc_library)) |
| [Rust](https://rules-proto-grpc.com/en/latest/lang/rust.html) | [rust_prost_proto_compile](https://rules-proto-grpc.com/en/latest/lang/rust.html#rust-prost-proto-compile) | Generates Rust protobuf ``.rs`` files using prost ([example](/example/rust/rust_prost_proto_compile)) |
| [Rust](https://rules-proto-grpc.com/en/latest/lang/rust.html) | [rust_tonic_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/rust.html#rust-tonic-grpc-compile) | Generates Rust protobuf and gRPC ``.rs`` files using prost and tonic ([example](/example/rust/rust_tonic_grpc_compile)) |
| [Rust](https://rules-proto-grpc.com/en/latest/lang/rust.html) | [rust_prost_proto_library](https://rules-proto-grpc.com/en/latest/lang/rust.html#rust-prost-proto-library) | Generates a Rust prost protobuf library using ``rust_library`` from ``rules_rust`` ([example](/example/rust/rust_prost_proto_library)) |
| [Rust](https://rules-proto-grpc.com/en/latest/lang/rust.html) | [rust_tonic_grpc_library](https://rules-proto-grpc.com/en/latest/lang/rust.html#rust-tonic-grpc-library) | Generates a Rust prost protobuf and tonic gRPC library using ``rust_library`` from ``rules_rust`` ([example](/example/rust/rust_tonic_grpc_library)) |
| [Scala](https://rules-proto-grpc.com/en/latest/lang/scala.html) | [scala_proto_compile](https://rules-proto-grpc.com/en/latest/lang/scala.html#scala-proto-compile) | Generates a Scala protobuf ``.jar`` file ([example](/example/scala/scala_proto_compile)) |
| [Scala](https://rules-proto-grpc.com/en/latest/lang/scala.html) | [scala_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/scala.html#scala-grpc-compile) | Generates Scala protobuf and gRPC ``.jar`` file ([example](/example/scala/scala_grpc_compile)) |
| [Scala](https://rules-proto-grpc.com/en/latest/lang/scala.html) | [scala_proto_library](https://rules-proto-grpc.com/en/latest/lang/scala.html#scala-proto-library) | Generates a Scala protobuf library using ``scala_library`` from ``rules_scala`` ([example](/example/scala/scala_proto_library)) |
| [Scala](https://rules-proto-grpc.com/en/latest/lang/scala.html) | [scala_grpc_library](https://rules-proto-grpc.com/en/latest/lang/scala.html#scala-grpc-library) | Generates a Scala protobuf and gRPC library using ``scala_library`` from ``rules_scala`` ([example](/example/scala/scala_grpc_library)) |
| [Swift](https://rules-proto-grpc.com/en/latest/lang/swift.html) | [swift_proto_compile](https://rules-proto-grpc.com/en/latest/lang/swift.html#swift-proto-compile) | Generates Swift protobuf ``.swift`` files ([example](/example/swift/swift_proto_compile)) |
| [Swift](https://rules-proto-grpc.com/en/latest/lang/swift.html) | [swift_grpc_compile](https://rules-proto-grpc.com/en/latest/lang/swift.html#swift-grpc-compile) | Generates Swift protobuf and gRPC ``.swift`` files ([example](/example/swift/swift_grpc_compile)) |
| [Swift](https://rules-proto-grpc.com/en/latest/lang/swift.html) | [swift_proto_library](https://rules-proto-grpc.com/en/latest/lang/swift.html#swift-proto-library) | Generates a Swift protobuf library using ``swift_library`` from ``rules_swift`` ([example](/example/swift/swift_proto_library)) |
| [Swift](https://rules-proto-grpc.com/en/latest/lang/swift.html) | [swift_grpc_library](https://rules-proto-grpc.com/en/latest/lang/swift.html#swift-grpc-library) | Generates a Swift protobuf and gRPC library using ``swift_library`` from ``rules_swift`` ([example](/example/swift/swift_grpc_library)) |

## License

This project is derived from [stackb/rules_proto](https://github.com/stackb/rules_proto) under the
[Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0) license and  this project therefore maintains the terms of that
license