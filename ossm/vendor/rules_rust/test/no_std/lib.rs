#![no_std]
// These features are needed to support no_std + alloc
#![feature(lang_items)]
#![feature(alloc_error_handler)]
#![feature(core_intrinsics)]
#![allow(unused_imports)]
use custom_alloc;

#[cfg(all(not(feature = "std"), not(test)))]
mod no_std;

#[cfg(not(feature = "std"))]
#[no_mangle]
pub extern "C" fn return_5_in_no_std() -> i32 {
    5
}

#[cfg(feature = "std")]
#[no_mangle]
pub extern "C" fn return_5_in_no_std() -> i32 {
    6
}
