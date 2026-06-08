use std::fs;
use std::path::Path;

use anyhow::{Context, Result};
use camino::Utf8PathBuf;
use clap::Parser;
use log::{debug, info};
use serde_json::{json, Value};
use vscode::{BazelInfo, LaunchConfigGenerator};

#[derive(Parser)]
#[command(
    name = "generate_launch_json",
    about = "Generate VSCode launch.json configurations for debugging Bazel Rust targets"
)]
struct Args {
    /// Space separated list of target patterns that comes after all other args.
    #[clap(default_value = "@//...")]
    targets: Vec<String>,

    /// Output file for launch configurations. If relative, the path will be joined with `workspace_root`.
    #[arg(short, long, default_value = ".vscode/launch.json")]
    output: Utf8PathBuf,

    /// Workspace root directory
    #[arg(long, env = "BUILD_WORKSPACE_DIRECTORY")]
    workspace_root: Option<Utf8PathBuf>,

    /// Path to Bazel binary
    #[arg(short, long, default_value = "bazel")]
    bazel: Utf8PathBuf,

    /// Only generate configurations (don't write files)
    #[arg(long)]
    dry_run: bool,

    /// Don't pretty print generated JSON
    #[arg(long)]
    no_pretty: bool,

    /// Replace entire file instead of merging (removes all existing configurations)
    #[arg(long)]
    replace: bool,
}

fn main() -> Result<()> {
    env_logger::init();

    let args = Args::parse();

    let workspace_root = args
        .workspace_root
        .unwrap_or_else(|| Utf8PathBuf::from("."));

    let bazel = args.bazel;
    let bazel_info = BazelInfo::try_new(&bazel, &workspace_root)?;

    let generator =
        LaunchConfigGenerator::new(workspace_root.clone(), bazel_info).with_bazel_binary(bazel);

    // Determine targets to process using query patterns
    let query_patterns = if args.targets.is_empty() {
        vec!["//...".to_string()] // Default to entire workspace
    } else {
        args.targets.clone()
    };

    info!(
        "Querying Rust targets with patterns: {}",
        query_patterns.join(", ")
    );
    let target_infos = generator
        .find_rust_targets(&query_patterns)
        .context("Failed to find Rust targets")?;
    info!(
        "Found {} Rust targets ({} binaries, {} tests)",
        target_infos.len(),
        target_infos.iter().filter(|t| !t.is_test).count(),
        target_infos.iter().filter(|t| t.is_test).count()
    );

    if target_infos.is_empty() {
        anyhow::bail!("No valid Rust targets found");
    }

    let mut launch_configs = Vec::new();

    for target_info in &target_infos {
        let config = generator.generate_launch_config(target_info)?;
        launch_configs.push(config);
        debug!("Generated config for {}", target_info.label);
    }

    // Make paths absolute relative to workspace root
    let output_path = if args.output.is_absolute() {
        args.output.clone()
    } else {
        workspace_root.join(&args.output)
    };

    // Create or merge launch.json
    let launch_data = if args.replace {
        // Replace mode: create completely new file
        json!({
            "version": "0.2.0",
            "configurations": launch_configs
        })
    } else {
        // Merge mode: merge with existing file (default)
        generator.merge_launch_configs(&launch_configs, &output_path)?
    };

    if args.dry_run {
        println!("=== launch.json ===");
        if args.no_pretty {
            println!("{}", serde_json::to_string(&launch_data)?);
        } else {
            println!("{}", serde_json::to_string_pretty(&launch_data)?);
        }
        return Ok(());
    }

    // Write file
    write_json_file(output_path.as_ref(), &launch_data, !args.no_pretty)
        .context("Failed to write launch.json")?;

    if args.replace {
        info!(
            "Generated {} launch configurations in {}",
            launch_configs.len(),
            args.output
        );
    } else {
        info!(
            "Merged {} launch configurations into {}",
            launch_configs.len(),
            args.output
        );
    }

    Ok(())
}

fn write_json_file(path: &str, data: &Value, pretty: bool) -> Result<()> {
    // Create parent directories if they don't exist
    if let Some(parent) = Path::new(path).parent() {
        fs::create_dir_all(parent).context("Failed to create output directory")?;
    }

    let json_str = if pretty {
        serde_json::to_string_pretty(data)?
    } else {
        serde_json::to_string(data)?
    };

    fs::write(path, json_str).context("Failed to write file")?;

    Ok(())
}
