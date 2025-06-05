mod src;
mod submodule;

pub fn forty_two_as_string() -> String {
    format!("{}", src::forty_two())
}

#[cfg(test)]
mod test {
    #[test]
    fn test_forty_two() {
        assert_eq!(super::src::forty_two(), 42);
    }

    #[test]
    fn test_forty_two_as_string() {
        assert_eq!(super::forty_two_as_string(), "42");
    }
}
