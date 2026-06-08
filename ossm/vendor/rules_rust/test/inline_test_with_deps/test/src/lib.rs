#[allow(dead_code)]
fn multiply(val: u32) -> u32 {
    val * 100
}

#[cfg(test)]
mod tests {
    use super::multiply;
    use dep::example_test_dep_fn;

    #[test]
    fn test() {
        assert_eq!(100, multiply(example_test_dep_fn()));
    }
}
