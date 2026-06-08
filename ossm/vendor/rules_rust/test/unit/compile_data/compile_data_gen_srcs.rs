pub mod generated;

/// Data loaded from compile data
pub const COMPILE_DATA: &str = include_str!("compile_data.txt");

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_compile_data_contents() {
        assert_eq!(COMPILE_DATA.trim_end(), "compile data contents");
    }

    #[test]
    fn test_generated_src() {
        assert_eq!(generated::GENERATED, "generated");
    }
}
