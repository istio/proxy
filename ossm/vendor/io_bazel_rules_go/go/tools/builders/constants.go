// Copyright 2024 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains constants used by nogo binaries.
// Note that this file is shared between the nogo binary and the builder.
// Sharing it as a library isn't possible as libraries depend on nogo, creating
// a circular dependency.
package main

// The exit codes for nogo binaries.
const (
	nogoSuccess int = iota
	nogoError
	nogoViolation
)
