#[test]
fn check_env_set() {
    assert_eq!("Howdy", env!("FROM_BUILD_SCRIPT"));
}
