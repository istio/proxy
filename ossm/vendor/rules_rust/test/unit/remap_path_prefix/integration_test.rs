#[test]
fn test_backtrace() {
    let runfiles = runfiles::Runfiles::create().unwrap();

    let binary_env = std::env::var("BINARY").unwrap();

    let binary_path = runfiles::rlocation!(runfiles, &binary_env).unwrap();

    eprintln!("Running {}", binary_path.display());

    let output = std::process::Command::new(binary_path)
        .env("RUST_BACKTRACE", "full")
        .output()
        .unwrap();
    let stderr = String::from_utf8(output.stderr).unwrap();

    eprintln!("Saw backtrace:\n{}", stderr);

    let mut check_next = false;
    for line in stderr.split('\n') {
        if check_next {
            if !line.contains("./test/unit/remap_path_prefix/panic_bin.rs:6:5") {
                panic!("Expected line to contain ./test/unit/remap_path_prefix/panic_bin.rs:6:5 but was {}", line);
            }
            return;
        }
        if line.contains("::do_call") {
            check_next = true;
        }
    }
    panic!("Didn't see expected line containing panic_bin::do_call");
}
