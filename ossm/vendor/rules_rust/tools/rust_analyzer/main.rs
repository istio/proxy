use std::collections::HashMap;
use std::env;
use std::path::PathBuf;
use std::process::Command;

use anyhow::anyhow;
use clap::Parser;
use gen_rust_project_lib::generate_crate_info;
use gen_rust_project_lib::write_rust_project;

// TODO(david): This shells out to an expected rule in the workspace root //:rust_analyzer that the user must define.
// It would be more convenient if it could automatically discover all the rust code in the workspace if this target
// does not exist.
fn main() -> anyhow::Result<()> {
    env_logger::init();

    let config = parse_config()?;

    let workspace_root = config
        .workspace
        .as_ref()
        .expect("failed to find workspace root, set with --workspace");

    let execution_root = config
        .execution_root
        .as_ref()
        .expect("failed to find execution root, is --execution-root set correctly?");

    let output_base = config
        .output_base
        .as_ref()
        .expect("failed to find output base, is -output-base set correctly?");

    let rules_rust_name = env!("ASPECT_REPOSITORY");

    // Generate the crate specs.
    generate_crate_info(
        &config.bazel,
        workspace_root,
        rules_rust_name,
        &config.targets,
    )?;

    // Use the generated files to write rust-project.json.
    write_rust_project(
        &config.bazel,
        workspace_root,
        &rules_rust_name,
        &config.targets,
        execution_root,
        output_base,
        workspace_root.join("rust-project.json"),
    )?;

    Ok(())
}

// Parse the configuration flags and supplement with bazel info as needed.
fn parse_config() -> anyhow::Result<Config> {
    let mut config = Config::parse();

    if config.workspace.is_some() && config.execution_root.is_some() {
        return Ok(config);
    }

    // We need some info from `bazel info`. Fetch it now.
    let mut bazel_info_command = Command::new(&config.bazel);
    bazel_info_command
        .env_remove("BAZELISK_SKIP_WRAPPER")
        .env_remove("BUILD_WORKING_DIRECTORY")
        .env_remove("BUILD_WORKSPACE_DIRECTORY")
        .arg("info");
    if let Some(workspace) = &config.workspace {
        bazel_info_command.current_dir(workspace);
    }

    // Execute bazel info.
    let output = bazel_info_command.output()?;
    if !output.status.success() {
        return Err(anyhow!(
            "Failed to run `bazel info` ({:?}): {}",
            output.status,
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    // Extract the output.
    let output = String::from_utf8_lossy(output.stdout.as_slice());
    let bazel_info = output
        .trim()
        .split('\n')
        .map(|line| line.split_at(line.find(':').expect("missing `:` in bazel info output")))
        .map(|(k, v)| (k, (v[1..]).trim()))
        .collect::<HashMap<_, _>>();

    if config.workspace.is_none() {
        config.workspace = bazel_info.get("workspace").map(Into::into);
    }
    if config.execution_root.is_none() {
        config.execution_root = bazel_info.get("execution_root").map(Into::into);
    }
    if config.output_base.is_none() {
        config.output_base = bazel_info.get("output_base").map(Into::into);
    }

    Ok(config)
}

#[derive(Debug, Parser)]
struct Config {
    /// The path to the Bazel workspace directory. If not specified, uses the result of `bazel info workspace`.
    #[clap(long, env = "BUILD_WORKSPACE_DIRECTORY")]
    workspace: Option<PathBuf>,

    /// The path to the Bazel execution root. If not specified, uses the result of `bazel info execution_root`.
    #[clap(long)]
    execution_root: Option<PathBuf>,

    /// The path to the Bazel output user root. If not specified, uses the result of `bazel info output_base`.
    #[clap(long, env = "OUTPUT_BASE")]
    output_base: Option<PathBuf>,

    /// The path to a Bazel binary
    #[clap(long, default_value = "bazel")]
    bazel: PathBuf,

    /// Space separated list of target patterns that comes after all other args.
    #[clap(default_value = "@//...")]
    targets: Vec<String>,
}
