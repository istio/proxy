use std::path::PathBuf;

#[test]
pub fn test_manifest_dir() {
    let cargo_manifest_dir =
        PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR is not set"));
    let path = cargo_manifest_dir.join("src/manifest_dir_file.txt");

    let actual = std::fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("Failed to read file: {}\n{:?}", path.display(), e));
    let actual = actual.trim_end();

    let expected = "This file tests that CARGO_MANIFEST_DIR is set for the test environment";
    assert_eq!(actual, expected);
}
