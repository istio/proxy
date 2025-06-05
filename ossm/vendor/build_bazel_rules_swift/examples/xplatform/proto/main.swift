// Copyright 2018 The Bazel Authors. All rights reserved.
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

import Foundation
import SwiftProtobuf
import examples_xplatform_proto_example_path_to_underscores_proto_swift
import examples_xplatform_proto_example_proto_swift

let person = RulesSwift_Examples_Person.with {
  $0.name = "Firstname Lastname"
  $0.age = 30
}

let data = try! person.serializedData()
print(Array(data))

let server = RulesSwift_Examples_Server.with {
  $0.name = "My Server"
  $0.api.name = "My API"
  let option = Google_Protobuf_Option.with {
    $0.name = "Person Option"
    if let value = try? Google_Protobuf_Any(message: person) {
      $0.value = value
    }
  }
  $0.api.options.append(option)
}

let data2 = try! server.serializedData()
print(Array(data2))
