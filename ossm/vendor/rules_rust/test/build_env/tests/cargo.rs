#[test]
fn cargo_env_vars() {
    assert_eq!(env!("CARGO_PKG_NAME"), "cargo_env-vars_test");
    assert_eq!(env!("CARGO_CRATE_NAME"), "cargo_env_vars_test");
    assert_eq!(
        env!("CARGO_PKG_NAME_FROM_BUILD_SCRIPT"),
        "cargo_build_script_env-vars"
    );
    assert_eq!(
        env!("CARGO_CRATE_NAME_FROM_BUILD_SCRIPT"),
        "cargo_build_script_env_vars"
    );
    assert_eq!(env!("HAS_TRAILING_SLASH"), "foo\\");
}
