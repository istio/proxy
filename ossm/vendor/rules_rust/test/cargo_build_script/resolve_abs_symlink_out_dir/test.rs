#[test]
pub fn test_compile_data_resolved_symlink() {
    let data = include_str!(concat!(env!("OUT_DIR"), "/data.txt"));
    assert_eq!("Resolved symlink file or relative symlink\n", data);
}

#[test]
pub fn test_compile_data_relative_symlink() {
    let data = include_str!(concat!(env!("OUT_DIR"), "/relative_symlink.txt"));
    assert_eq!("Resolved symlink file or relative symlink\n", data);
}

#[test]
pub fn test_compile_data_relative_nested_symlink() {
    let data = include_str!(concat!(env!("OUT_DIR"), "/nested/relative_symlink.txt"));
    assert_eq!("Resolved symlink file or relative symlink\n", data);
}
