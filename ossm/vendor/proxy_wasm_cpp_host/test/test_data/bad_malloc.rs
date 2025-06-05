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

use std::mem::MaybeUninit;

extern "C" {
    fn proxy_log(level: u32, message_data: *const u8, message_size: usize) -> u32;
    fn proxy_done() -> u32;
}

#[no_mangle]
pub extern "C" fn proxy_abi_version_0_2_0() {}

#[no_mangle]
pub extern "C" fn proxy_on_memory_allocate(size: usize) -> *mut u8 {
    let mut vec: Vec<MaybeUninit<u8>> = Vec::with_capacity(size);
    unsafe {
        vec.set_len(size);
    }
    match size {
        0xAAAA => {
            let message = "this is fine";
            unsafe {
                proxy_log(0, message.as_ptr(), message.len());
            }
        }
        0xBBBB => {
            unsafe {
                proxy_done();
            }
        }
        _ => {}
    }
    let slice = vec.into_boxed_slice();
    Box::into_raw(slice) as *mut u8
}
