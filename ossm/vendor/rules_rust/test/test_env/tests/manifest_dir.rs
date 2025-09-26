#[test]
pub fn test_manifest_dir() {
    let path = format!(
        "{}/src/manifest_dir_file.txt",
        std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR is not set")
    );
    let actual = std::fs::read_to_string(std::path::Path::new(&path)).expect("Failed to read file");
    let actual = actual.trim_end();
    let expected = "This file tests that CARGO_MANIFEST_DIR is set for the test environment";
    assert_eq!(actual, expected);
}
