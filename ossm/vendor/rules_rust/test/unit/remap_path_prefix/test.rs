#[test]
fn test_dep_file_name() {
    // The following commit changed the output of `--remap-path-prefix` so for the purposes of this test
    // both possible outputs (before and after the change) are tested
    // https://github.com/rust-lang/rust/commit/8cbfb26383347cf4970a488177acfdc35c30984b

    let given_str = dep::get_file_name::<()>();

    let expected_str = "test/unit/remap_path_prefix/dep.rs";

    let mut legacy_path = std::path::PathBuf::from(".");
    // After the ., the path components appear to be joined with / on all platforms.
    // This is probably a rustc bug we should report.
    legacy_path.push(expected_str);
    let legacy_expected_str = legacy_path.to_str().unwrap();

    assert!(given_str == expected_str || given_str == legacy_expected_str, "src file name does not match any available options\n  given:    `{}`\n  expected: `{}`\n  legacy:   `{}`", given_str, expected_str, legacy_expected_str);
}
