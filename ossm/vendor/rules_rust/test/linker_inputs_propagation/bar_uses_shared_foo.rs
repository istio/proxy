extern "C" {
    pub fn foo() -> i32;
}

/** Safety doc.

  # Safety

*/
#[no_mangle]
pub unsafe extern "C" fn double_foo() -> i32 {
    2 * foo()
}
