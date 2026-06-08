#[test]
fn test_genquery_file_empty() {
    let r = runfiles::Runfiles::create().unwrap();
    let file = runfiles::rlocation!(r, std::env::var("GENQUERY_OUTPUT").unwrap()).unwrap();
    let content = std::fs::read_to_string(&file)
        .unwrap_or_else(|e| panic!("Failed to read file: {}\n{:?}", file.display(), e));
    assert_eq!("", content);
}
