//! cargo_bazel integration tests that run Cargo to test generating metadata.

extern crate cargo_bazel;
extern crate serde_json;
extern crate tempfile;

use std::collections::HashMap;
use std::env;
use std::fs;
use std::path::PathBuf;

use anyhow::{ensure, Context, Result};
use cargo_bazel::cli::{splice, SpliceOptions};
use serde_json::{json, Value};

fn should_skip_test() -> bool {
    // All test cases require network access to build pull crate metadata
    // so that we can actually run `cargo tree`. However, RBE (and perhaps
    // other environments) disallow or don't support this. In those cases,
    // we just skip this test case.
    use std::net::ToSocketAddrs;
    if "github.com:443".to_socket_addrs().is_err() {
        eprintln!("This test case requires network access.");
        true
    } else {
        false
    }
}

fn setup_cargo_env(rfiles: &runfiles::Runfiles) -> Result<(PathBuf, PathBuf)> {
    let cargo = runfiles::rlocation!(
        rfiles,
        env::var("CARGO").context("CARGO environment variable must be set.")?
    )
    .unwrap();
    let rustc = runfiles::rlocation!(
        rfiles,
        env::var("RUSTC").context("RUSTC environment variable must be set.")?
    )
    .unwrap();
    ensure!(cargo.exists());
    ensure!(rustc.exists());
    // If $RUSTC is a relative path it can cause issues with
    // `cargo_metadata::MetadataCommand`. Just to be on the safe side, we make
    // both of these env variables absolute paths.
    if cargo != PathBuf::from(env::var("CARGO").unwrap()) {
        env::set_var("CARGO", cargo.as_os_str());
    }
    if rustc != PathBuf::from(env::var("RUSTC").unwrap()) {
        env::set_var("RUSTC", rustc.as_os_str());
    }

    let cargo_home = PathBuf::from(
        env::var("TEST_TMPDIR").context("TEST_TMPDIR environment variable must be set.")?,
    )
    .join("cargo_home");
    env::set_var("CARGO_HOME", cargo_home.as_os_str());
    fs::create_dir_all(&cargo_home)?;

    println!("Environment:");
    println!("\tRUSTC={}", rustc.display());
    println!("\tCARGO={}", cargo.display());
    println!("\tCARGO_HOME={}", cargo_home.display());

    Ok((cargo, rustc))
}

fn run(repository_name: &str, manifests: HashMap<String, String>, lockfile: &str) -> Value {
    let scratch = tempfile::tempdir().unwrap();
    let runfiles = runfiles::Runfiles::create().unwrap();

    let (cargo, rustc) = setup_cargo_env(&runfiles).unwrap();

    let splicing_manifest = scratch.path().join("splicing_manifest.json");
    fs::write(
        &splicing_manifest,
        serde_json::to_string(&json!({
            "manifests": manifests,
            "direct_packages": {},
            "resolver_version": "2"
        }))
        .unwrap(),
    )
    .unwrap();

    let config = scratch.path().join("config.json");
    fs::write(
        &config,
        serde_json::to_string(&json!({
            "generate_binaries": false,
            "generate_build_scripts": false,
            "rendering": {
                "repository_name": repository_name,
                "regen_command": "//crate_universe:cargo_integration_test"
            },
            "supported_platform_triples": [
                "wasm32-unknown-unknown",
                "x86_64-apple-darwin",
                "x86_64-pc-windows-msvc",
                "x86_64-unknown-linux-gnu",
            ]
        }))
        .unwrap(),
    )
    .unwrap();

    splice(SpliceOptions {
        splicing_manifest,
        cargo_lockfile: Some(runfiles::rlocation!(runfiles, lockfile).unwrap()),
        repin: None,
        workspace_dir: None,
        output_dir: scratch.path().join("out"),
        dry_run: false,
        cargo_config: None,
        config,
        cargo,
        rustc,
        nonhermetic_root_bazel_workspace_dir: PathBuf::from("/doesnotexist"),
    })
    .unwrap();

    let metadata = serde_json::from_str::<Value>(
        &fs::read_to_string(scratch.path().join("out").join("metadata.json")).unwrap(),
    )
    .unwrap();

    metadata
}

// See crate_universe/test_data/metadata/target_features/Cargo.toml for input.
#[test]
fn feature_generator() {
    if should_skip_test() {
        eprintln!("Skipping!");
        return;
    }

    let r = runfiles::Runfiles::create().unwrap();
    let metadata = run(
        "target_feature_test",
        HashMap::from([(
            runfiles::rlocation!(
                r,
                "rules_rust/crate_universe/test_data/metadata/target_features/Cargo.toml"
            )
            .unwrap()
            .to_string_lossy()
            .to_string(),
            "//:test_input".to_string(),
        )]),
        "rules_rust/crate_universe/test_data/metadata/target_features/Cargo.lock",
    );

    assert_eq!(
        json!({
            "common": {
                "deps": [
                    "arrayvec 0.7.2",
                    "bitflags 1.3.2",
                    "fxhash 0.2.1",
                    "log 0.4.17",
                    "naga 0.10.0",
                    "parking_lot 0.12.1",
                    "profiling 1.0.7",
                    "raw-window-handle 0.5.0",
                    "thiserror 1.0.37",
                    "wgpu-types 0.14.1",
                ],
                "features": [
                    "default",
                ],
            },
            "selects": {
                "x86_64-apple-darwin": {
                    "deps": [
                        "block 0.1.6",
                        "core-graphics-types 0.1.1",
                        "foreign-types 0.3.2",
                        "metal 0.24.0",
                        "objc 0.2.7",
                    ],
                    "features": [
                        "block",
                        "foreign-types",
                        "metal",
                    ],
                },
                "x86_64-pc-windows-msvc": {
                    "deps": [
                        "ash 0.37.1+1.3.235",
                        "bit-set 0.5.3",
                        "d3d12 0.5.0",
                        "gpu-alloc 0.5.3",
                        "gpu-descriptor 0.2.3",
                        "libloading 0.7.4",
                        "range-alloc 0.1.2",
                        "renderdoc-sys 0.7.1",
                        "smallvec 1.10.0",
                        "winapi 0.3.9",
                    ],
                    "features": [
                        "ash",
                        "bit-set",
                        "dx11",
                        "dx12",
                        "gpu-alloc",
                        "gpu-descriptor",
                        "libloading",
                        "native",
                        "range-alloc",
                        "renderdoc",
                        "renderdoc-sys",
                        "smallvec",
                        "vulkan",
                    ],
                },
                "x86_64-unknown-linux-gnu": {
                    "deps": [
                        "ash 0.37.1+1.3.235",
                        "glow 0.11.2",
                        "gpu-alloc 0.5.3",
                        "gpu-descriptor 0.2.3",
                        "khronos-egl 4.1.0",
                        "libloading 0.7.4",
                        "renderdoc-sys 0.7.1",
                        "smallvec 1.10.0",
                    ],
                    "features": [
                        "ash",
                        "egl",
                        "gles",
                        "glow",
                        "gpu-alloc",
                        "gpu-descriptor",
                        "libloading",
                        "renderdoc",
                        "renderdoc-sys",
                        "smallvec",
                        "vulkan",
                    ],
                },
            },
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"]["wgpu-hal 0.14.1"],
    );
}

// See crate_universe/test_data/metadata/target_cfg_features/Cargo.toml for input.
#[test]
fn feature_generator_cfg_features() {
    if should_skip_test() {
        eprintln!("Skipping!");
        return;
    }

    let r = runfiles::Runfiles::create().unwrap();
    let metadata = run(
        "target_cfg_features_test",
        HashMap::from([(
            runfiles::rlocation!(
                r,
                "rules_rust/crate_universe/test_data/metadata/target_cfg_features/Cargo.toml"
            )
            .unwrap()
            .to_string_lossy()
            .to_string(),
            "//:test_input".to_string(),
        )]),
        "rules_rust/crate_universe/test_data/metadata/target_cfg_features/Cargo.lock",
    );

    assert_eq!(
        json!({
            "autocfg 1.1.0": {
                "selects": {},
            },
            "pin-project-lite 0.2.9": {
                "selects": {},
            },
            "target_cfg_features 0.1.0": {
                "common": {
                    "deps": [
                        "tokio 1.25.0",
                    ],
                },
                "selects": {},
            },
            "tokio 1.25.0": {
                "common": {
                    "deps": [
                        "autocfg 1.1.0",
                        "pin-project-lite 0.2.9",
                    ],
                    "features": [
                        "default",
                    ],
                },
                // Note: "x86_64-pc-windows-msvc" is *not* here, despite
                // being included in `supported_platform_triples` above!
                "selects": {
                    "x86_64-apple-darwin": {
                        "features": [
                            "fs",
                        ],
                    },
                    "x86_64-unknown-linux-gnu": {
                        "features": [
                            "fs",
                        ],
                    },
                },
            },
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"],
    );
}

#[test]
fn feature_generator_workspace() {
    if should_skip_test() {
        eprintln!("Skipping!");
        return;
    }

    let r = runfiles::Runfiles::create().unwrap();
    let metadata = run(
        "workspace_test",
        HashMap::from([
            (
                runfiles::rlocation!(
                    r,
                    "rules_rust/crate_universe/test_data/metadata/workspace/Cargo.toml"
                )
                .unwrap()
                .to_string_lossy()
                .to_string(),
                "//:test_input".to_string(),
            ),
            (
                runfiles::rlocation!(
                    r,
                    "rules_rust/crate_universe/test_data/metadata/workspace/child/Cargo.toml"
                )
                .unwrap()
                .to_string_lossy()
                .to_string(),
                "//crate_universe:test_data/metadata/workspace/child/Cargo.toml".to_string(),
            ),
        ]),
        "rules_rust/crate_universe/test_data/metadata/workspace/Cargo.lock",
    );

    assert!(!metadata["metadata"]["cargo-bazel"]["tree_metadata"]["wgpu 0.14.0"].is_null());
}

#[test]
fn feature_generator_crate_combined_features() {
    if should_skip_test() {
        eprintln!("Skipping!");
        return;
    }

    let r = runfiles::Runfiles::create().unwrap();
    let metadata = run(
        "crate_combined_features",
        HashMap::from([(
            runfiles::rlocation!(
                r,
                "rules_rust/crate_universe/test_data/metadata/crate_combined_features/Cargo.toml"
            )
            .unwrap()
            .to_string_lossy()
            .to_string(),
            "//:test_input".to_string(),
        )]),
        "rules_rust/crate_universe/test_data/metadata/crate_combined_features/Cargo.lock",
    );

    // serde appears twice in the list of dependencies, with and without derive features
    assert_eq!(
        json!({
            "deps": [
                "serde_derive 1.0.158",
            ],
            "features": [
                "default",
                "derive",
                "serde_derive",
                "std",
            ],
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"]["serde 1.0.158"]["common"],
    );
}

// See crate_universe/test_data/metadata/target_cfg_features/Cargo.toml for input.
#[test]
fn resolver_2_deps() {
    if should_skip_test() {
        eprintln!("Skipping!");
        return;
    }

    let r = runfiles::Runfiles::create().unwrap();
    let metadata = run(
        "resolver_2_deps_test",
        HashMap::from([(
            runfiles::rlocation!(
                r,
                "rules_rust/crate_universe/test_data/metadata/resolver_2_deps/Cargo.toml"
            )
            .unwrap()
            .to_string_lossy()
            .to_string(),
            "//:test_input".to_string(),
        )]),
        "rules_rust/crate_universe/test_data/metadata/resolver_2_deps/Cargo.lock",
    );

    assert_eq!(
        json!({
            "common": {
                "deps": [
                    "bytes 1.6.0",
                    "pin-project-lite 0.2.14",
                ],
                "features": [
                    "bytes",
                    "default",
                    "io-util",
                ],
            },
            // Note that there is no `wasm32-unknown-unknown` entry since all it's dependencies
            // are common. Also note that `mio` is unique to these platforms as it's something
            // that should be excluded from Wasm platforms.
            "selects": {
                "x86_64-apple-darwin": {
                    "deps": [
                        "libc 0.2.153",
                        "mio 0.8.11",
                        "socket2 0.5.7",
                    ],
                    "features": [
                        "io-std",
                        "libc",
                        "mio",
                        "net",
                        "rt",
                        "socket2",
                        "sync",
                        "time",
                    ],
                },
                "x86_64-pc-windows-msvc": {
                    "deps": [
                        "mio 0.8.11",
                        "socket2 0.5.7",
                        "windows-sys 0.48.0",
                    ],
                    "features": [
                        "io-std",
                        "libc",
                        "mio",
                        "net",
                        "rt",
                        "socket2",
                        "sync",
                        "time",
                        "windows-sys",
                    ],
                },
                "x86_64-unknown-linux-gnu": {
                    "deps": [
                        "libc 0.2.153",
                        "mio 0.8.11",
                        "socket2 0.5.7",
                    ],
                    "features": [
                        "io-std",
                        "libc",
                        "mio",
                        "net",
                        "rt",
                        "socket2",
                        "sync",
                        "time",
                    ],
                },
            },
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"]["tokio 1.37.0"],
    );

    assert_eq!(
        json!({
            // Note linux is not present since linux has no unique dependencies or features
            // for this crate.
            "selects": {
                "wasm32-unknown-unknown": {
                    "deps": [
                        "js-sys 0.3.69",
                        "wasm-bindgen 0.2.92",
                    ],
                },
                "x86_64-apple-darwin": {
                    "deps": [
                        "core-foundation-sys 0.8.6",
                    ],
                },
                "x86_64-pc-windows-msvc": {
                    "deps": [
                        "windows-core 0.52.0",
                    ],
                },
            },
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"]["iana-time-zone 0.1.60"],
    );
}

#[test]
fn host_specific_build_deps() {
    if should_skip_test() {
        eprintln!("Skipping!");
        return;
    }

    let r = runfiles::Runfiles::create().unwrap();

    let src_cargo_toml = runfiles::rlocation!(
        r,
        "rules_rust/crate_universe/test_data/metadata/host_specific_build_deps/Cargo.toml"
    )
    .unwrap();

    // Put Cargo.toml into writable directory structure and create target/ directory to verify that
    // cargo does not incorrectly cache rustc info in target/.rustc_info.json file.
    let scratch = tempfile::tempdir().unwrap();
    let cargo_toml = scratch.path().join("Cargo.toml");
    fs::copy(src_cargo_toml, &cargo_toml).unwrap();
    fs::create_dir(scratch.path().join("target")).unwrap();

    let metadata = run(
        "host_specific_build_deps",
        HashMap::from([(
            cargo_toml.to_string_lossy().to_string(),
            "//:test_input".to_string(),
        )]),
        "rules_rust/crate_universe/test_data/metadata/host_specific_build_deps/Cargo.lock",
    );

    assert_eq!(
        json!({
            "common": {
                "deps": [
                    "bitflags 2.6.0",
                ],
                "features": [
                    "alloc",
                    "default",
                    "fs",
                    "libc-extra-traits",
                    "std",
                    "use-libc-auxv",
                ],
            },
            // Note that there is no `wasm32-unknown-unknown` or `x86_64-pc-windows-msvc` entry
            // since these platforms do not depend on `rustix`. The chain breaks due to the
            // conditions here: https://github.com/Stebalien/tempfile/blob/v3.11.0/Cargo.toml#L25-L33
            "selects": {
                "x86_64-apple-darwin": {
                    "deps": [
                        "errno 0.3.9",
                        "libc 0.2.158",
                    ],
                },
                "x86_64-unknown-linux-gnu": {
                    "deps": [
                        "linux-raw-sys 0.4.14",
                    ],
                },
            },
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"]["rustix 0.38.36"],
    );

    assert_eq!(
        json!({
            "common": {
                "deps": [
                    "cfg-if 1.0.0",
                    "fastrand 2.1.1",
                    "once_cell 1.19.0",
                ],
            },
            // Note that windows does not contain `rustix` and instead `windows-sys`.
            // This shows correct detection of exec platform constraints.
            "selects": {
                "x86_64-apple-darwin": {
                    "deps": [
                        "rustix 0.38.36",
                    ],
                },
                "x86_64-pc-windows-msvc": {
                    "deps": [
                        "windows-sys 0.59.0",
                    ],
                },
                "x86_64-unknown-linux-gnu": {
                    "deps": [
                        "rustix 0.38.36",
                    ],
                },
            },
        }),
        metadata["metadata"]["cargo-bazel"]["tree_metadata"]["tempfile 3.12.0"],
    );
}
