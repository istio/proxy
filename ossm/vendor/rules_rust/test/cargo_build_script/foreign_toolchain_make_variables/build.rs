fn main() {
    assert_eq!(std::env::var("FROM_TOOLCHAIN").unwrap(), "present");
    assert_eq!(
        std::env::var("MODIFIED_FROM_TOOLCHAIN").unwrap(),
        "modifiedpresent"
    );
    // This was not explicitly forwarded by the cargo_build_script target, so should not be present.
    assert!(std::env::var_os("ALSO_FROM_TOOLCHAIN").is_none());
}
