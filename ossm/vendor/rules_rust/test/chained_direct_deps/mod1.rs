pub fn world() -> String {
    "world".to_owned()
}

#[cfg(test)]
mod test {
    #[test]
    fn test_world() {
        assert_eq!(super::world(), "world");
    }
}
