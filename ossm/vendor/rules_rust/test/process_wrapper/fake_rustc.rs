//! This binary mocks the output of rustc when run with `--error-format=json` and `--json=artifacts`.

fn main() {
    let should_error = std::env::args().any(|arg| arg == "error");

    eprintln!(r#"{{"rendered": "should be\nin output"}}"#);
    if should_error {
        eprintln!("ERROR!\nthis should all\nappear in output.");
        std::process::exit(1);
    }
    eprintln!(r#"{{"emit": "metadata"}}"#);
    std::thread::sleep(std::time::Duration::from_secs(1));
    eprintln!(r#"{{"rendered": "should not be in output"}}"#);
}
