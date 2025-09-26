#[test]
fn test_dep_file_name() {
    assert_eq!(
        dep::get_file_name::<()>(),
        "test/unit/remap_path_prefix/dep.rs"
    );
}
