use proc_macro_definition::make_forty_two;
use proc_macro_with_native_dep::make_forty_two_from_native_dep;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_forty_two() {
        assert_eq!(make_forty_two!(), 42);
    }

    #[test]
    fn test_forty_two_from_native_dep() {
        assert_eq!(make_forty_two_from_native_dep!(), 42);
    }
}
