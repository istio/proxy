#[test]
pub fn test_compile_data() {
    let data = include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/", env!("DATA")));

    assert_eq!("La-Li-Lu-Le-Lo\n", data);
}
