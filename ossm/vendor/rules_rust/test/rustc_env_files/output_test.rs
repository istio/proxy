use std::process;

#[test]
fn output_test() {
    let r = runfiles::Runfiles::create().unwrap();
    let binary = runfiles::rlocation!(r, env!("HELLO_ENV")).unwrap();

    let output = process::Command::new(binary)
        .output()
        .expect("Failed to spawn process");
    if !output.status.success() {
        eprintln!("Failed to execute binary");
        eprintln!("{}", std::str::from_utf8(&output.stdout).unwrap());
        eprintln!("{}", std::str::from_utf8(&output.stderr).unwrap());
        process::exit(output.status.code().unwrap());
    }

    let stdout = std::str::from_utf8(&output.stdout).unwrap().trim();
    assert_eq!("Howdy from version 1.2.3", stdout);
}
