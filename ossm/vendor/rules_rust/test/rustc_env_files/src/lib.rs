pub fn from_lib() -> &'static str {
    env!("GREETING")
}

#[cfg(test)]
mod tests {
    #[test]
    fn verify_from_lib() {
        assert_eq!(super::from_lib(), "Howdy");
    }

    #[test]
    fn verify_from_test() {
        assert_eq!(env!("GREETING"), "Howdy");
    }
}
