/* Copyright 2019 The Bazel Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package multi_suffix_compiler

import (
	"testing"

	"github.com/bazelbuild/rules_go/tests/core/go_proto_library/enum"
)

func use(interface{}) {}

func TestMultiSuffixCompiler(t *testing.T) {
	// This test expects the compiler to generate two outputs:
	// <proto>.pb.go and <proto>_dbenums.pb.go.
	// Assert <proto>_dbenums.pb.go contains String() that returns dbenum value.
	v := enum.Enum_BYTES
	expected := "bytes_type"
	if v.String() != expected {
		panic(v.String())
	}
	// Assert <proto>.pb.go contains String() that returns proto Enum key.
	v = enum.Enum_INT32
	expected = "INT32"
	if v.String() != expected {
		panic(v.String())
	}
}
