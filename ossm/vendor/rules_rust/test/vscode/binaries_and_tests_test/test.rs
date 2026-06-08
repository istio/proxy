#[test]
fn test_integration() {
    let result = mylib::greet("Integration");
    assert!(result.contains("Integration"));
}
