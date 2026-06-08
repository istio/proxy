use std::env;

#[test]
fn can_find_the_out_dir_file() {
    // The file contents must be included via a macro.
    let contents = include_str!(concat!(env!("OUT_DIR"), "/test_content.txt"));
    assert_eq!("Test content", contents);
}

#[test]
fn no_out_dir_at_runtime() {
    // Cargo seems to set this at runtime as well, although the documentation
    // says it's only available at compile time.
    assert!(env::var("OUT_DIR").is_err());
}
