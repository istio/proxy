//! Tests for the boostrap process wrapper

use std::fs::read_to_string;

use runfiles::Runfiles;

/// Test that the shell process wrapper starts with the expected shebang to
/// avoid breaking the contract with the `bootstrap_process_wrapper` rule.
#[test]
fn test_shebang() {
    let rfiles = Runfiles::create().unwrap();

    let script = runfiles::rlocation!(
        rfiles,
        "rules_rust/util/process_wrapper/private/process_wrapper.sh"
    )
    .unwrap();

    let content = read_to_string(script).unwrap();
    assert!(
        content.starts_with("#!/usr/bin/env bash"),
        "The shell script does not start with the expected shebang."
    )
}
