/// Data loaded from compile data
pub const COMPILE_DATA: &str = include_str!(env!("COMPILE_DATA_PATH"));

#[cfg(test)]
mod test {
    use super::*;

    /// A test that is expected to be compiled from a target that does not
    /// directly populate the `compile_data` attribute
    #[test]
    fn test_compile_data_contents() {
        assert_eq!(COMPILE_DATA.trim_end(), "compile data contents");
    }

    /// An extra module that tests the `rust_test` rule wrapping the
    /// `rust_library` is able to provide it's own compile data.
    #[cfg(test_compile_data)]
    mod test_compile_data {
        const TEST_COMPILE_DATA: &str = include_str!("test_compile_data.txt");

        #[test]
        fn test_compile_data_contents() {
            assert_eq!(TEST_COMPILE_DATA.trim_end(), "test compile data contents");
        }
    }
}
