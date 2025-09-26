//! A Cargo build script binary used in unit tests for the Bazel `cargo_build_script` rule

/// `cargo_build_script` should always set `CARGO_ENCODED_RUSTFLAGS`
fn test_encoded_rustflags() {
    let encoded_rustflags = std::env::var("CARGO_ENCODED_RUSTFLAGS").unwrap();

    let flags: Vec<String> = encoded_rustflags
        .split('\x1f')
        .map(str::to_string)
        .collect();
    assert_eq!(flags.len(), 2);

    assert!(flags[0].starts_with("--sysroot"));

    // Ensure the `pwd` template has been resolved
    assert!(!flags[0].contains("${pwd}"));

    assert_eq!(flags[1], "--cfg=foo=\"bar\"");
}

/// Ensure Make variables provided by the `toolchains` attribute are expandable.
fn test_toolchain_var() {
    let tool = std::env::var("EXPANDED_TOOLCHAIN_VAR").unwrap();
    if cfg!(target_os = "windows") {
        assert!(tool.ends_with("rustc.exe"));
    } else {
        assert!(tool.ends_with("rustc"));
    }
    eprintln!("{}", std::env::current_dir().unwrap().display());
    let tool_path = std::path::PathBuf::from(tool);
    assert!(tool_path.exists(), "{} does not exist", tool_path.display());
}

fn main() {
    // Perform some unit testing
    test_encoded_rustflags();
    test_toolchain_var();

    // Pass the TOOL_PATH along to the rust_test so we can assert on it.
    println!(
        "cargo:rustc-env=TOOL_PATH={}",
        std::env::var("TOOL").unwrap()
    );

    // Assert that the CC, CXX and LD env vars existed and were executable.
    // We don't assert what happens when they're executed (in particular, we don't check for a
    // non-zero exit code), but this asserts that it's an existing file which is executable.
    for env_var in &["CC", "CXX", "LD"] {
        let path = std::env::var(env_var)
            .unwrap_or_else(|err| panic!("Error getting {}: {}", env_var, err));
        std::process::Command::new(path).status().unwrap();
    }

    // Assert that some env variables are set.
    for env_var in &["CFLAGS", "CXXFLAGS", "LDFLAGS"] {
        assert!(std::env::var(env_var).is_ok());
    }

    assert_eq!(std::env::var("CARGO_MANIFEST_LINKS").unwrap(), "beep");
}
