#[test]
fn cargo_env_vars() {
    assert_eq!(
        env!("CARGO_PKG_NAME"),
        "cargo-env-vars-custom-crate-name-test"
    );
    assert_eq!(env!("CARGO_CRATE_NAME"), "custom_crate_name");
}
