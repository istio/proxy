#[test]
pub fn test_data() {
    assert_eq!(
        "La-Li-Lu-Le-Lo",
        transitive::data(),
        "A consumer of the `lib` crate consumed this crate without the `build.rs` generated feature."
    );
}
