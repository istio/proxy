// Copyright 2020 The Bazel Authors. All rights reserved.
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

extern crate libc;

extern "C" {
    pub fn return_zero() -> libc::c_int;
}

fn main() {
    let zero = unsafe { return_zero() };
    println!("Got {zero} from our shared lib");
}

#[cfg(test)]
mod test {
    extern "C" {
        pub fn return_zero() -> libc::c_int;
    }

    #[test]
    fn test_return_zero() {
        assert_eq!(0, unsafe { return_zero() });
        // If we make it past this call, it linked correctly, so the test passes.
    }
}
