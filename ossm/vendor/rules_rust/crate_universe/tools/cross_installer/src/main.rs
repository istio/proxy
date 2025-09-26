//! A utility for cross compiling binaries using Cross

use std::path::{Path, PathBuf};
use std::process::{self, Command};
use std::{env, fs};

use clap::Parser;

#[derive(Parser, Debug)]
struct Options {
    /// The path to an artifacts directory expecting to contain directories
    /// named after platform tripes with binaries inside.
    #[clap(long)]
    pub(crate) output: PathBuf,

    /// A url prefix where the artifacts can be found
    #[clap(long)]
    pub(crate) target: String,
}

/// Prepare cross for building.
fn prepare_workspace(workspace_root: &Path) {
    // Unfortunately, cross runs into issues when cross compiling incrementally.
    // To avoid this, the workspace must be cleaned
    let cargo = env::current_dir().unwrap().join(env!("CARGO"));
    Command::new(cargo)
        .current_dir(workspace_root)
        .arg("clean")
        .status()
        .unwrap();
}

/// Execute a build for the provided platform
fn execute_cross(working_dir: &Path, target_triple: &str) {
    let cross = env::current_dir().unwrap().join(env!("CROSS_BIN"));
    let status = Command::new(cross)
        .current_dir(working_dir)
        .arg("build")
        .arg("--release")
        .arg("--locked")
        .arg("--bin")
        .arg("cargo-bazel")
        .arg(format!("--target={target_triple}"))
        // https://github.com/cross-rs/cross/issues/1447
        .env("CROSS_NO_WARNINGS", "0")
        .status()
        .unwrap();

    if !status.success() {
        process::exit(status.code().unwrap_or(1));
    }
}

/// Install results to the output directory
fn install_outputs(working_dir: &Path, triple: &str, output_dir: &Path) {
    let is_windows_target = triple.contains("windows");
    let binary_name = if is_windows_target {
        "cargo-bazel.exe"
    } else {
        "cargo-bazel"
    };

    // Since we always build from the workspace root, and the output
    // is always expected to be `./target/{triple}`, we build a path
    // to the expected output and write it.
    let artifact = working_dir
        .join("target")
        .join(triple)
        .join("release")
        .join(binary_name);

    let dest = output_dir.join(triple).join(binary_name);
    fs::create_dir_all(dest.parent().unwrap()).unwrap();
    fs::rename(artifact, &dest).unwrap();
    println!("Installed: {}", dest.display());
}

fn main() {
    let opt = Options::parse();

    // Locate the workspace root
    let workspace_root = PathBuf::from(
        env::var("BUILD_WORKSPACE_DIRECTORY")
            .expect("cross_installer is designed to run under Bazel"),
    )
    .join("crate_universe");

    // Do some setup
    prepare_workspace(&workspace_root);

    // Build the binary
    execute_cross(&workspace_root, &opt.target);

    // Install the results
    install_outputs(&workspace_root, &opt.target, &opt.output);
}
