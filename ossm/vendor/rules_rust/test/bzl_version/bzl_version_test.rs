//! A test to ensure the rules_rust bzlmod versions match the standard versions.

use runfiles::Runfiles;

fn parse_module_bazel_version(text: &str) -> String {
    let mut found_module = false;
    for line in text.split('\n') {
        if found_module {
            assert!(!line.ends_with(')'), "Failed to parse version");
            if let Some((param, value)) = line.rsplit_once(" = ") {
                if param.trim() == "version" {
                    return value.trim().trim_matches(',').trim_matches('"').to_owned();
                }
            }
        } else if line.starts_with("module(") {
            found_module = true;
            continue;
        }
    }
    panic!("Failed to find MODULE.bazel version");
}

/// If this test fails it means `//:version.bzl` and `//:MODULE.bazel` need to
/// be synced up. `//:version.bzl` should contain the source of truth.
#[test]
fn module_bzl_has_correct_version() {
    let version = std::env::var("VERSION").unwrap();
    let module_bazel_text = {
        let r = Runfiles::create().unwrap();
        let path = runfiles::rlocation!(r, std::env::var("MODULE_BAZEL").unwrap()).unwrap();
        std::fs::read_to_string(path).unwrap()
    };

    let module_bazel_version = parse_module_bazel_version(&module_bazel_text);

    assert_eq!(
        version, module_bazel_version,
        "@rules_rust//:version.bzl and //:MODULE.bazel versions are out of sync"
    );
}
