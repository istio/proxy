#[test]
pub fn test_manifest_dir() {
    let actual = include_str!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/src/manifest_dir_file.txt"
    ))
    .trim_end();
    let expected = "This file tests that CARGO_MANIFEST_DIR is set for the build environment";
    assert_eq!(actual, expected);
}
