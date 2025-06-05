/// Here we are trying to use renamed dependency
/// inside a doc string to check rust_doc_test
/// ```
/// use aliased_adder;
///  assert_eq!(43, aliased_adder::inc(42));
/// ```
pub fn inc(n: u32) -> u32 {
    aliased_adder::inc(n)
}
