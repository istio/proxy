#[cfg(feature = "with_proc_macro")]
use rustdoc_proc_macro::make_answer;

#[cfg(feature = "with_proc_macro")]
make_answer!();

#[cfg(feature = "with_build_script")]
pub const CONST: &str = env!("CONST");

/// The answer to the ultimate question
/// ```
/// fn answer() -> u32 { 42 }
/// assert_eq!(answer(), 42);
/// ```
#[cfg(not(feature = "with_proc_macro"))]
pub fn answer() -> u32 {
    42
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_answer() {
        assert_eq!(answer(), 42);
    }
}
