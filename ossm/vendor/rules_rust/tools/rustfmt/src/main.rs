//! A tool for querying Rust source files wired into Bazel and running Rustfmt on them.

use std::collections::HashMap;
use std::env;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::str;

/// The Bazel Rustfmt tool entry point
fn main() {
    // Gather all command line and environment settings
    let options = parse_args();

    // Gather a list of all formattable targets
    let targets = query_rustfmt_targets(&options);

    // Run rustfmt on these targets
    apply_rustfmt(&options, &targets);
}

/// The edition to use in cases where the default edition is unspecified by Bazel
const FALLBACK_EDITION: &str = "2018";

/// Determine the Rust edition to use in cases where a target has not explicitly
/// specified the edition via an `edition` attribute.
fn get_default_edition() -> &'static str {
    if !env!("RUST_DEFAULT_EDITION").is_empty() {
        env!("RUST_DEFAULT_EDITION")
    } else {
        FALLBACK_EDITION
    }
}

/// Get a list of all editions to run formatting for
fn get_editions() -> Vec<String> {
    vec!["2015".to_owned(), "2018".to_owned(), "2021".to_owned()]
}

/// Run a bazel command, capturing stdout while streaming stderr to surface errors
fn bazel_command(bazel_bin: &Path, args: &[String], current_dir: &Path) -> Vec<String> {
    let child = Command::new(bazel_bin)
        .current_dir(current_dir)
        .args(args)
        .stdout(Stdio::piped())
        .stderr(Stdio::inherit())
        .spawn()
        .expect("Failed to spawn bazel command");

    let output = child
        .wait_with_output()
        .expect("Failed to wait on spawned command");

    if !output.status.success() {
        eprintln!("Failed to perform `bazel query` command.");
        std::process::exit(output.status.code().unwrap_or(1));
    }

    str::from_utf8(&output.stdout)
        .expect("Invalid stream from command")
        .split('\n')
        .filter(|line| !line.is_empty())
        .map(|line| line.to_string())
        .collect()
}

/// The regex representation of an empty `edition` attribute
const EMPTY_EDITION: &str = "^$";

/// Query for all `*.rs` files in a workspace that are dependencies of targets with the requested edition.
fn edition_query(bazel_bin: &Path, edition: &str, scope: &str, current_dir: &Path) -> Vec<String> {
    let query_args = vec![
        "query".to_owned(),
        // Query explanation:
        // Filter all local targets ending in `*.rs`.
        //     Get all source files.
        //         Get direct dependencies.
        //             Get all targets with the specified `edition` attribute.
        //             Except for targets tagged with `norustfmt`, `no-rustfmt`, or `no-format`.
        //             And except for targets with a populated `crate` attribute since `crate` defines edition for this target
        format!(
            r#"let scope = set({scope}) in filter("^//.*\.rs$", kind("source file", deps(attr(edition, "{edition}", $scope) except attr(tags, "(^\[|, )(no-format|no-rustfmt|norustfmt)(, |\]$)", $scope) except attr(crate, ".*", $scope), 1)))"#,
        ),
        "--keep_going".to_owned(),
        "--noimplicit_deps".to_owned(),
    ];

    bazel_command(bazel_bin, &query_args, current_dir)
}

/// Perform a `bazel` query to determine all source files which are to be
/// formatted for particular Rust editions.
fn query_rustfmt_targets(options: &Config) -> HashMap<String, Vec<String>> {
    let scope = options
        .packages
        .clone()
        .into_iter()
        .reduce(|acc, item| acc + " " + &item)
        .unwrap_or_else(|| "//...:all".to_owned());

    let editions = get_editions();
    let default_edition = get_default_edition();

    editions
        .into_iter()
        .map(|edition| {
            let mut targets = edition_query(&options.bazel, &edition, &scope, &options.workspace);

            // For all targets relying on the toolchain for it's edition,
            // query anything with an unset edition
            if edition == default_edition {
                targets.extend(edition_query(
                    &options.bazel,
                    EMPTY_EDITION,
                    &scope,
                    &options.workspace,
                ))
            }

            (edition, targets)
        })
        .collect()
}

/// Run rustfmt on a set of Bazel targets
fn apply_rustfmt(options: &Config, editions_and_targets: &HashMap<String, Vec<String>>) {
    // There is no work to do if the list of targets is empty
    if editions_and_targets.is_empty() {
        return;
    }

    for (edition, targets) in editions_and_targets.iter() {
        if targets.is_empty() {
            continue;
        }

        // Get paths to all formattable sources
        let sources: Vec<String> = targets
            .iter()
            .map(|target| target.replace(':', "/").trim_start_matches('/').to_owned())
            .collect();

        // Run rustfmt
        let status = Command::new(&options.rustfmt_config.rustfmt)
            .current_dir(&options.workspace)
            .arg("--edition")
            .arg(edition)
            .arg("--config-path")
            .arg(&options.rustfmt_config.config)
            .args(sources)
            .status()
            .expect("Failed to run rustfmt");

        if !status.success() {
            std::process::exit(status.code().unwrap_or(1));
        }
    }
}

/// A struct containing details used for executing rustfmt.
#[derive(Debug)]
struct Config {
    /// The path of the Bazel workspace root.
    pub workspace: PathBuf,

    /// The Bazel executable to use for builds and queries.
    pub bazel: PathBuf,

    /// Information about the current rustfmt binary to run.
    pub rustfmt_config: rustfmt_lib::RustfmtConfig,

    /// Optionally, users can pass a list of targets/packages/scopes
    /// (eg `//my:target` or `//my/pkg/...`) to control the targets
    /// to be formatted. If empty, all targets in the workspace will
    /// be formatted.
    pub packages: Vec<String>,
}

/// Parse command line arguments and environment variables to
/// produce config data for running rustfmt.
fn parse_args() -> Config {
    Config{
        workspace: PathBuf::from(
            env::var("BUILD_WORKSPACE_DIRECTORY")
            .expect("The environment variable BUILD_WORKSPACE_DIRECTORY is required for finding the workspace root")
        ),
        bazel: PathBuf::from(
            env::var("BAZEL_REAL")
            .unwrap_or_else(|_| "bazel".to_owned())
        ),
        rustfmt_config: rustfmt_lib::parse_rustfmt_config(),
        packages: env::args().skip(1).collect(),
    }
}
