pub mod generated;

#[cfg(test)]
mod tests {
    #[test]
    fn test_forty_two() {
        assert_eq!(super::generated::forty_two(), 42);
    }
}
