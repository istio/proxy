extern "C" {
    pub fn foo() -> i32;
}

/** Safety doc.

  # Safety

*/
pub fn double_foo() -> i32 {
    2 * unsafe { foo() }
}
