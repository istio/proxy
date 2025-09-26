//! A test to ensure the rules_rust extension bzlmod core version is accurate.

use runfiles::Runfiles;

fn parse_rules_rust_version(text: &str) -> String {
    let mut in_module = false;
    let mut found_dep = false;
    for line in text.split('\n') {
        if in_module {
            if line.ends_with(')') {
                in_module = false;
                continue;
            }

            if found_dep {
                if let Some((param, value)) = line.rsplit_once(" = ") {
                    if param.trim() == "version" {
                        return value.trim().trim_matches(',').trim_matches('"').to_owned();
                    }
                }
            }

            if line.trim().starts_with("name = \"rules_rust\"") {
                found_dep = true;
                continue;
            }

            continue;
        }

        if line.starts_with("bazel_dep(") {
            assert!(
                !found_dep,
                "Found `rules_rust` dep but couldn't determine version."
            );
            in_module = true;
            continue;
        }
    }
    panic!("Failed to find rules_rust version in:\n```\n{}\n```", text);
}

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
    let r = Runfiles::create().unwrap();

    let core_version = {
        let path = runfiles::rlocation!(r, std::env::var("CORE_MODULE_BAZEL").unwrap()).unwrap();
        let text = std::fs::read_to_string(path).unwrap();
        parse_module_bazel_version(&text)
    };
    let requested_version = {
        let path = runfiles::rlocation!(r, std::env::var("MODULE_BAZEL").unwrap()).unwrap();
        let text = std::fs::read_to_string(path).unwrap();
        parse_rules_rust_version(&text)
    };

    assert_eq!(
        core_version, requested_version,
        "Core rules_rust and the dependency for the current module are out of sync."
    );
}
