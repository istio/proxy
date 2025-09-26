#[test]
pub fn test_arbitrary_env() {
    let actual = env!("USER_DEFINED_KEY");
    let expected = "USER_DEFINED_VALUE".to_owned();
    assert_eq!(actual, expected);
}
