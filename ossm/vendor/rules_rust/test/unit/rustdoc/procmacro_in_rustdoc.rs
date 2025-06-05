/// A different answer that only exists if using procmacros.
/// ```
/// use rustdoc_proc_macro::make_answer;
/// make_answer!();
/// assert_eq!(answer(), 42);
/// ```
pub fn procmacro_answer() -> u32 {
    24
}
