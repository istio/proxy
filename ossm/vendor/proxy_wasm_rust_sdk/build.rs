// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=RUSTFLAGS");
    println!("cargo:rustc-check-cfg=cfg(wasi_exec_model_reactor)");

    if let Some(target_os) = std::env::var_os("CARGO_CFG_TARGET_OS") {
        if target_os != "wasi" {
            return;
        }
    }

    if let Some(rustflags) = std::env::var_os("CARGO_ENCODED_RUSTFLAGS") {
        for flag in rustflags.to_string_lossy().split('\x1f') {
            if flag.ends_with("wasi-exec-model=reactor") {
                println!("cargo:rustc-cfg=wasi_exec_model_reactor");
                return;
            }
        }
    }
}
