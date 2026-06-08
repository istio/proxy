pub fn from_lib() -> &'static str {
    env!("USER_DEFINED_KEY")
}

#[cfg(test)]
mod tests {
    #[test]
    fn verify_from_lib() {
        assert_eq!(super::from_lib(), "USER_DEFINED_VALUE");
    }

    #[test]
    fn verify_from_test() {
        assert_eq!(env!("USER_DEFINED_KEY"), "USER_DEFINED_VALUE");
    }
}
