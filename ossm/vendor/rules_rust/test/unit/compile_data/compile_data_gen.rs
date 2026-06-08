/// Data loaded from generated compile data
pub const COMPILE_DATA: &str = include_str!("generated.txt");

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_compile_data_contents() {
        assert_eq!(COMPILE_DATA.trim_end(), "generated compile data contents");
    }
}
