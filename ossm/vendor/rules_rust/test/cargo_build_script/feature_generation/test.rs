#[test]
pub fn test_data() {
    assert_eq!(
        "La-Li-Lu-Le-Lo",
        lib::DATA,
        "The `lib` crate was not compiled with the feature produced from it's `build.rs`"
    );
}
