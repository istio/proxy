.PHONY: android_android_proto_compile_example
android_android_proto_compile_example:
	cd example/android/android_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: android_android_grpc_compile_example
android_android_grpc_compile_example:
	cd example/android/android_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: android_android_proto_library_example
android_android_proto_library_example:
	cd example/android/android_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: android_android_grpc_library_example
android_android_grpc_library_example:
	cd example/android/android_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: android_examples
android_examples: android_android_proto_compile_example android_android_grpc_compile_example android_android_proto_library_example android_android_grpc_library_example

.PHONY: buf_buf_proto_breaking_test_example
buf_buf_proto_breaking_test_example:
	cd example/buf/buf_proto_breaking_test; \
	bazel --batch test --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --test_output=errors --disk_cache=../../bazel-disk-cache //...

.PHONY: buf_buf_proto_lint_test_example
buf_buf_proto_lint_test_example:
	cd example/buf/buf_proto_lint_test; \
	bazel --batch test --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --test_output=errors --disk_cache=../../bazel-disk-cache //...

.PHONY: buf_examples
buf_examples: buf_buf_proto_breaking_test_example buf_buf_proto_lint_test_example

.PHONY: c_c_proto_compile_example
c_c_proto_compile_example:
	cd example/c/c_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: c_c_proto_library_example
c_c_proto_library_example:
	cd example/c/c_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: c_examples
c_examples: c_c_proto_compile_example c_c_proto_library_example

.PHONY: cpp_cpp_proto_compile_example
cpp_cpp_proto_compile_example:
	cd example/cpp/cpp_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: cpp_cpp_grpc_compile_example
cpp_cpp_grpc_compile_example:
	cd example/cpp/cpp_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: cpp_cpp_proto_library_example
cpp_cpp_proto_library_example:
	cd example/cpp/cpp_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: cpp_cpp_grpc_library_example
cpp_cpp_grpc_library_example:
	cd example/cpp/cpp_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: cpp_examples
cpp_examples: cpp_cpp_proto_compile_example cpp_cpp_grpc_compile_example cpp_cpp_proto_library_example cpp_cpp_grpc_library_example

.PHONY: csharp_csharp_proto_compile_example
csharp_csharp_proto_compile_example:
	cd example/csharp/csharp_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: csharp_csharp_grpc_compile_example
csharp_csharp_grpc_compile_example:
	cd example/csharp/csharp_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: csharp_csharp_proto_library_example
csharp_csharp_proto_library_example:
	cd example/csharp/csharp_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: csharp_csharp_grpc_library_example
csharp_csharp_grpc_library_example:
	cd example/csharp/csharp_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: csharp_examples
csharp_examples: csharp_csharp_proto_compile_example csharp_csharp_grpc_compile_example csharp_csharp_proto_library_example csharp_csharp_grpc_library_example

.PHONY: d_d_proto_compile_example
d_d_proto_compile_example:
	cd example/d/d_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: d_d_proto_library_example
d_d_proto_library_example:
	cd example/d/d_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: d_examples
d_examples: d_d_proto_compile_example d_d_proto_library_example

.PHONY: doc_doc_docbook_compile_example
doc_doc_docbook_compile_example:
	cd example/doc/doc_docbook_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: doc_doc_html_compile_example
doc_doc_html_compile_example:
	cd example/doc/doc_html_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: doc_doc_json_compile_example
doc_doc_json_compile_example:
	cd example/doc/doc_json_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: doc_doc_markdown_compile_example
doc_doc_markdown_compile_example:
	cd example/doc/doc_markdown_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: doc_doc_template_compile_example
doc_doc_template_compile_example:
	cd example/doc/doc_template_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: doc_examples
doc_examples: doc_doc_docbook_compile_example doc_doc_html_compile_example doc_doc_json_compile_example doc_doc_markdown_compile_example doc_doc_template_compile_example

.PHONY: fsharp_fsharp_proto_compile_example
fsharp_fsharp_proto_compile_example:
	cd example/fsharp/fsharp_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: fsharp_fsharp_grpc_compile_example
fsharp_fsharp_grpc_compile_example:
	cd example/fsharp/fsharp_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: fsharp_fsharp_proto_library_example
fsharp_fsharp_proto_library_example:
	cd example/fsharp/fsharp_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: fsharp_fsharp_grpc_library_example
fsharp_fsharp_grpc_library_example:
	cd example/fsharp/fsharp_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: fsharp_examples
fsharp_examples: fsharp_fsharp_proto_compile_example fsharp_fsharp_grpc_compile_example fsharp_fsharp_proto_library_example fsharp_fsharp_grpc_library_example

.PHONY: go_go_proto_compile_example
go_go_proto_compile_example:
	cd example/go/go_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: go_go_grpc_compile_example
go_go_grpc_compile_example:
	cd example/go/go_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: go_go_validate_compile_example
go_go_validate_compile_example:
	cd example/go/go_validate_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: go_go_proto_library_example
go_go_proto_library_example:
	cd example/go/go_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: go_go_grpc_library_example
go_go_grpc_library_example:
	cd example/go/go_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: go_go_validate_library_example
go_go_validate_library_example:
	cd example/go/go_validate_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: go_examples
go_examples: go_go_proto_compile_example go_go_grpc_compile_example go_go_validate_compile_example go_go_proto_library_example go_go_grpc_library_example go_go_validate_library_example

.PHONY: grpc-gateway_gateway_grpc_compile_example
grpc-gateway_gateway_grpc_compile_example:
	cd example/grpc-gateway/gateway_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: grpc-gateway_gateway_openapiv2_compile_example
grpc-gateway_gateway_openapiv2_compile_example:
	cd example/grpc-gateway/gateway_openapiv2_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: grpc-gateway_gateway_grpc_library_example
grpc-gateway_gateway_grpc_library_example:
	cd example/grpc-gateway/gateway_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: grpc-gateway_examples
grpc-gateway_examples: grpc-gateway_gateway_grpc_compile_example grpc-gateway_gateway_openapiv2_compile_example grpc-gateway_gateway_grpc_library_example

.PHONY: java_java_proto_compile_example
java_java_proto_compile_example:
	cd example/java/java_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: java_java_grpc_compile_example
java_java_grpc_compile_example:
	cd example/java/java_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: java_java_proto_library_example
java_java_proto_library_example:
	cd example/java/java_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: java_java_grpc_library_example
java_java_grpc_library_example:
	cd example/java/java_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: java_examples
java_examples: java_java_proto_compile_example java_java_grpc_compile_example java_java_proto_library_example java_java_grpc_library_example

.PHONY: js_js_proto_compile_example
js_js_proto_compile_example:
	cd example/js/js_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: js_js_grpc_node_compile_example
js_js_grpc_node_compile_example:
	cd example/js/js_grpc_node_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: js_js_grpc_web_compile_example
js_js_grpc_web_compile_example:
	cd example/js/js_grpc_web_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: js_js_proto_library_example
js_js_proto_library_example:
	cd example/js/js_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: js_js_grpc_node_library_example
js_js_grpc_node_library_example:
	cd example/js/js_grpc_node_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: js_js_grpc_web_library_example
js_js_grpc_web_library_example:
	cd example/js/js_grpc_web_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: js_examples
js_examples: js_js_proto_compile_example js_js_grpc_node_compile_example js_js_grpc_web_compile_example js_js_proto_library_example js_js_grpc_node_library_example js_js_grpc_web_library_example

.PHONY: objc_objc_proto_compile_example
objc_objc_proto_compile_example:
	cd example/objc/objc_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: objc_objc_grpc_compile_example
objc_objc_grpc_compile_example:
	cd example/objc/objc_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: objc_objc_proto_library_example
objc_objc_proto_library_example:
	cd example/objc/objc_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: objc_objc_grpc_library_example
objc_objc_grpc_library_example:
	cd example/objc/objc_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: objc_examples
objc_examples: objc_objc_proto_compile_example objc_objc_grpc_compile_example objc_objc_proto_library_example objc_objc_grpc_library_example

.PHONY: php_php_proto_compile_example
php_php_proto_compile_example:
	cd example/php/php_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: php_php_grpc_compile_example
php_php_grpc_compile_example:
	cd example/php/php_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: php_examples
php_examples: php_php_proto_compile_example php_php_grpc_compile_example

.PHONY: python_python_proto_compile_example
python_python_proto_compile_example:
	cd example/python/python_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: python_python_grpc_compile_example
python_python_grpc_compile_example:
	cd example/python/python_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: python_python_grpclib_compile_example
python_python_grpclib_compile_example:
	cd example/python/python_grpclib_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: python_python_proto_library_example
python_python_proto_library_example:
	cd example/python/python_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: python_python_grpc_library_example
python_python_grpc_library_example:
	cd example/python/python_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: python_python_grpclib_library_example
python_python_grpclib_library_example:
	cd example/python/python_grpclib_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: python_examples
python_examples: python_python_proto_compile_example python_python_grpc_compile_example python_python_grpclib_compile_example python_python_proto_library_example python_python_grpc_library_example python_python_grpclib_library_example

.PHONY: ruby_ruby_proto_compile_example
ruby_ruby_proto_compile_example:
	cd example/ruby/ruby_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: ruby_ruby_grpc_compile_example
ruby_ruby_grpc_compile_example:
	cd example/ruby/ruby_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: ruby_ruby_proto_library_example
ruby_ruby_proto_library_example:
	cd example/ruby/ruby_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: ruby_ruby_grpc_library_example
ruby_ruby_grpc_library_example:
	cd example/ruby/ruby_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: ruby_examples
ruby_examples: ruby_ruby_proto_compile_example ruby_ruby_grpc_compile_example ruby_ruby_proto_library_example ruby_ruby_grpc_library_example

.PHONY: rust_rust_prost_proto_compile_example
rust_rust_prost_proto_compile_example:
	cd example/rust/rust_prost_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: rust_rust_tonic_grpc_compile_example
rust_rust_tonic_grpc_compile_example:
	cd example/rust/rust_tonic_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: rust_rust_prost_proto_library_example
rust_rust_prost_proto_library_example:
	cd example/rust/rust_prost_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: rust_rust_tonic_grpc_library_example
rust_rust_tonic_grpc_library_example:
	cd example/rust/rust_tonic_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: rust_examples
rust_examples: rust_rust_prost_proto_compile_example rust_rust_tonic_grpc_compile_example rust_rust_prost_proto_library_example rust_rust_tonic_grpc_library_example

.PHONY: scala_scala_proto_compile_example
scala_scala_proto_compile_example:
	cd example/scala/scala_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: scala_scala_grpc_compile_example
scala_scala_grpc_compile_example:
	cd example/scala/scala_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: scala_scala_proto_library_example
scala_scala_proto_library_example:
	cd example/scala/scala_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: scala_scala_grpc_library_example
scala_scala_grpc_library_example:
	cd example/scala/scala_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: scala_examples
scala_examples: scala_scala_proto_compile_example scala_scala_grpc_compile_example scala_scala_proto_library_example scala_scala_grpc_library_example

.PHONY: swift_swift_proto_compile_example
swift_swift_proto_compile_example:
	cd example/swift/swift_proto_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: swift_swift_grpc_compile_example
swift_swift_grpc_compile_example:
	cd example/swift/swift_grpc_compile; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: swift_swift_proto_library_example
swift_swift_proto_library_example:
	cd example/swift/swift_proto_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: swift_swift_grpc_library_example
swift_swift_grpc_library_example:
	cd example/swift/swift_grpc_library; \
	bazel --batch build --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 ${BAZEL_EXTRA_FLAGS} --verbose_failures --disk_cache=../../bazel-disk-cache //...

.PHONY: swift_examples
swift_examples: swift_swift_proto_compile_example swift_swift_grpc_compile_example swift_swift_proto_library_example swift_swift_grpc_library_example

.PHONY: all_examples
all_examples: android_android_proto_compile_example android_android_grpc_compile_example android_android_proto_library_example android_android_grpc_library_example buf_buf_proto_breaking_test_example buf_buf_proto_lint_test_example c_c_proto_compile_example c_c_proto_library_example cpp_cpp_proto_compile_example cpp_cpp_grpc_compile_example cpp_cpp_proto_library_example cpp_cpp_grpc_library_example csharp_csharp_proto_compile_example csharp_csharp_grpc_compile_example csharp_csharp_proto_library_example csharp_csharp_grpc_library_example d_d_proto_compile_example d_d_proto_library_example doc_doc_docbook_compile_example doc_doc_html_compile_example doc_doc_json_compile_example doc_doc_markdown_compile_example doc_doc_template_compile_example fsharp_fsharp_proto_compile_example fsharp_fsharp_grpc_compile_example fsharp_fsharp_proto_library_example fsharp_fsharp_grpc_library_example go_go_proto_compile_example go_go_grpc_compile_example go_go_validate_compile_example go_go_proto_library_example go_go_grpc_library_example go_go_validate_library_example grpc-gateway_gateway_grpc_compile_example grpc-gateway_gateway_openapiv2_compile_example grpc-gateway_gateway_grpc_library_example java_java_proto_compile_example java_java_grpc_compile_example java_java_proto_library_example java_java_grpc_library_example js_js_proto_compile_example js_js_grpc_node_compile_example js_js_grpc_web_compile_example js_js_proto_library_example js_js_grpc_node_library_example js_js_grpc_web_library_example objc_objc_proto_compile_example objc_objc_grpc_compile_example objc_objc_proto_library_example objc_objc_grpc_library_example php_php_proto_compile_example php_php_grpc_compile_example python_python_proto_compile_example python_python_grpc_compile_example python_python_grpclib_compile_example python_python_proto_library_example python_python_grpc_library_example python_python_grpclib_library_example ruby_ruby_proto_compile_example ruby_ruby_grpc_compile_example ruby_ruby_proto_library_example ruby_ruby_grpc_library_example rust_rust_prost_proto_compile_example rust_rust_tonic_grpc_compile_example rust_rust_prost_proto_library_example rust_rust_tonic_grpc_library_example scala_scala_proto_compile_example scala_scala_grpc_compile_example scala_scala_proto_library_example scala_scala_grpc_library_example swift_swift_proto_compile_example swift_swift_grpc_compile_example swift_swift_proto_library_example swift_swift_grpc_library_example
