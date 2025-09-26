//! A Cargo build script binary used in unit tests for the Bazel `cargo_build_script` rule

use std::collections::HashSet;
use std::path::PathBuf;

fn main() {
    // The cargo_build_script macro appends an underscore to the given name.
    //
    // This file would be the only expected source file within the CARGO_MANIFEST_DIR without
    // any exec root symlink functionality.
    let build_script = PathBuf::from(
        std::env::args()
            .next()
            .expect("Unable to get the build script executable"),
    );

    let build_script_name = build_script
        .file_name()
        .expect("Unable to get the build script name")
        .to_str()
        .expect("Unable to convert the build script name to a string");

    let mut root_files = std::fs::read_dir(".")
        .expect("Unable to read the current directory")
        .map(|entry| {
            entry
                .expect("Failed to get entry")
                .file_name()
                .into_string()
                .expect("Failed to convert file name to string")
        })
        .collect::<HashSet<_>>();

    assert!(
        root_files.remove(build_script_name),
        "Build script must be in the current directory"
    );

    // An implementation detail of `cargo_build_script` is that is has two
    // intermediate targets which represent the script runner. The script
    // itself is suffixed with `_` while it's wrapper is suffixed with `-`.
    // ensure the script is removed from consideration before continuing the
    // test.
    let alt_script_name = if cfg!(windows) {
        format!(
            "{}_.exe",
            &build_script_name[0..(build_script_name.len() - ".exe".len() - 1)],
        )
    } else {
        format!("{}_", &build_script_name[0..(build_script_name.len() - 1)],)
    };
    assert!(
        root_files.remove(&alt_script_name),
        "Failed to remove {}",
        alt_script_name
    );

    let cargo_manifest_dir_file = root_files.take("cargo_manifest_dir_file.txt");
    assert!(
        cargo_manifest_dir_file.is_some(),
        "'cargo_manifest_dir_file.txt' must be in the current directory"
    );
    assert_eq!(
        std::fs::read_to_string(cargo_manifest_dir_file.unwrap()).unwrap(),
        "This is a file to be found alongside the build script."
    );

    if symlink_feature_enabled() {
        assert!(
            root_files.take("bazel-out").is_some(),
            "'bazel-out' must be in the current directory when the symlink feature is enabled"
        );
        assert!(
            root_files.take("external").is_some(),
            "'external' must be in the current directory when the symlink feature is enabled"
        );
    }

    let remaining_files = root_files
        .iter()
        // An __action_home_<hash> directory is created in some remote execution builds.
        .filter(|file| !file.starts_with("__action_home"))
        .collect::<HashSet<_>>();

    // If we're in a sandbox then there should be no other files in the current directory.
    let is_in_sandbox = is_in_sandbox(&root_files);
    assert_eq!(
        remaining_files.is_empty(),
        is_in_sandbox,
        "There should not be any other files in the current directory, found {:?}",
        root_files
    );
}

/// Check if the symlink feature is enabled.
fn symlink_feature_enabled() -> bool {
    std::env::var("RULES_RUST_SYMLINK_EXEC_ROOT")
        .map(|v| v == "1")
        .unwrap_or(false)
}

/// Check if the current directory is in a sandbox.
///
/// This is done by checking if the current directory contains a directory prefixed with
/// `local-spawn-runner`. If it does, then it is assumed to not be in a sandbox.
///
/// Non-sandboxed builds contain one or more directories in the exec root with the following
/// structure:
///   local-spawn-runner.6722268259075335658/
///   `-- work/
///   local-spawn-runner.3585764808440126801/
///   `-- work/
fn is_in_sandbox(cwd_files: &HashSet<String>) -> bool {
    for file in cwd_files {
        if file.starts_with("local-spawn-runner.") {
            return false;
        }
    }

    true
}
