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

#[no_mangle]
pub extern "C" fn proxy_abi_version_0_2_0() {}

#[no_mangle]
pub extern "C" fn proxy_on_memory_allocate(_: usize) -> *mut u8 {
    std::ptr::null_mut()
}

// TODO(PiotrSikora): switch to "getrandom" crate.
pub mod wasi_snapshot_preview1 {
    #[link(wasm_import_module = "wasi_snapshot_preview1")]
    extern "C" {
        pub fn random_get(buf: *mut u8, buf_len: usize) -> u16;
    }
}

#[no_mangle]
pub extern "C" fn run(size: usize) {
    let mut buf: Vec<u8> = Vec::with_capacity(size);
    match unsafe { wasi_snapshot_preview1::random_get(buf.as_mut_ptr(), size) } {
        0 => println!("random_get({}) succeeded.", size),
        _ => println!("random_get({}) failed.", size),
    }
}
