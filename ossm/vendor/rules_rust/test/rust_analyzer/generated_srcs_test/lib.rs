pub mod generated;

#[cfg(test)]
mod tests {
    #[test]
    fn test_fourty_two() {
        assert_eq!(super::generated::forty_two(), 42);
    }
}
