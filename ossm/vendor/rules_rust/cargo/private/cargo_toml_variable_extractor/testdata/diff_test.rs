#[test]
fn diff_test() {
    let runfiles = runfiles::Runfiles::create().unwrap();

    let file_1_env = std::env::var("FILE_1").unwrap();
    let file_2_env = std::env::var("FILE_2").unwrap();

    let file_1 = runfiles::rlocation!(runfiles, &file_1_env).unwrap();
    let file_2 = runfiles::rlocation!(runfiles, &file_2_env).unwrap();

    let file_1_content = std::fs::read_to_string(&file_1).unwrap();
    let file_2_content = std::fs::read_to_string(&file_2).unwrap();

    let file_1_lines = file_1_content.split_whitespace().collect::<Vec<&str>>();
    let file_2_lines = file_2_content.split_whitespace().collect::<Vec<&str>>();

    assert_eq!(
        file_1_lines,
        file_2_lines,
        "Files `{}` and `{}` differ.",
        file_1.display(),
        file_2.display()
    );
}
