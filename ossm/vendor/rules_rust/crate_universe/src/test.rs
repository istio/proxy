//! A module containing common test helpers

use std::path::PathBuf;

pub(crate) fn mock_cargo_metadata_package() -> cargo_metadata::Package {
    serde_json::from_value(serde_json::json!({
        "name": "mock-pkg",
        "version": "3.3.3",
        "id": "mock-pkg 3.3.3 (registry+https://github.com/rust-lang/crates.io-index)",
        "license": "Unlicense/MIT",
        "license_file": null,
        "description": "Fast multiple substring searching.",
        "source": "registry+https://github.com/rust-lang/crates.io-index",
        "dependencies": [],
        "targets": [],
        "features": {},
        "manifest_path": "/tmp/mock-pkg-3.3.3/Cargo.toml",
        "metadata": null,
        "publish": null,
        "authors": [],
        "categories": [],
        "keywords": [],
        "readme": "README.md",
        "repository": "",
        "homepage": "",
        "documentation": null,
        "edition": "2021",
        "links": null,
        "default_run": null
    }))
    .unwrap()
}

pub(crate) fn mock_cargo_lock_package() -> cargo_lock::Package {
    toml::from_str(&textwrap::dedent(
        r#"
        name = "mock-pkg"
        version = "3.3.3"
        source = "registry+https://github.com/rust-lang/crates.io-index"
        checksum = "ee49baf6cb617b853aa8d93bf420db2383fab46d314482ca2803b40d5fde979b"
        dependencies = []
        "#,
    ))
    .unwrap()
}

/// Create a temp directory that is conditionally leaked when running under Bazel.
/// Bazel will cleanup the test temp directory after tests have finished.
pub(crate) fn test_tempdir(prefix: &str) -> (Option<tempfile::TempDir>, PathBuf) {
    match std::env::var("TEST_TMPDIR") {
        Ok(t) => {
            let dir = tempfile::TempDir::with_prefix_in(prefix, t).unwrap();
            (None, dir.into_path())
        }
        Err(_) => {
            let dir = tempfile::TempDir::with_prefix(prefix).unwrap();
            let path = PathBuf::from(dir.path());
            (Some(dir), path)
        }
    }
}

pub(crate) mod metadata {
    pub(crate) fn alias() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/aliases/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn build_scripts() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/build_scripts/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn crate_types() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_types/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn multi_cfg_dep() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/multi_cfg_dep/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn multi_kind_proc_macro_dep() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/multi_kind_proc_macro_dep/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn no_deps() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/no_deps/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn path_patching() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/path_patching/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn optional_deps_disabled() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_optional_deps_disabled/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn renamed_optional_deps_disabled() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_renamed_optional_deps_disabled/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn optional_deps_disabled_build_dep_enabled() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_optional_deps_disabled_build_dep_enabled/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn optional_deps_enabled() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_optional_deps_enabled/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn renamed_optional_deps_enabled() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_renamed_optional_deps_enabled/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn common() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/common/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn example_proc_macro_dep() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/example_proc_macro_dep/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn git_repos() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/git_repos/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn has_package_metadata() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/has_package_metadata/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn resolver_2_deps() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/resolver_2_deps/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn abspath() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/abspath/metadata.json"
        )))
        .unwrap()
    }

    pub(crate) fn workspace_build_scripts_deps() -> cargo_metadata::Metadata {
        serde_json::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/workspace_build_scripts_deps/metadata.json"
        )))
        .unwrap()
    }
}

pub(crate) mod lockfile {
    use std::str::FromStr;

    pub(crate) fn alias() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/aliases/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn build_scripts() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/build_scripts/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn crate_types() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/crate_types/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn multi_cfg_dep() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/multi_cfg_dep/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn no_deps() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/no_deps/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn path_patching() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/path_patching/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn common() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/common/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn git_repos() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/git_repos/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn has_package_metadata() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/has_package_metadata/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn abspath() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/abspath/Cargo.lock"
        )))
        .unwrap()
    }

    pub(crate) fn workspace_build_scripts_deps() -> cargo_lock::Lockfile {
        cargo_lock::Lockfile::from_str(include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/metadata/workspace_build_scripts_deps/Cargo.lock"
        )))
        .unwrap()
    }
}
