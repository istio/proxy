//! Validates a rust-project.json file by invoking rust-analyzer

use clap::Parser;
use env_logger::Builder;
use log::{debug, error, info, LevelFilter};
use runfiles::{rlocation, Runfiles};
use std::env;
use std::path::PathBuf;
use std::process::{exit, Command};

#[derive(Parser, Debug)]
#[command(
    author,
    version,
    about = "Validates a rust-project.json file using rust-analyzer"
)]
struct Args {
    /// Path to the rust-project.json file to validate
    project: Option<PathBuf>,

    /// Verbose output from rust-analyzer
    #[arg(short, long)]
    verbose: bool,
}

fn main() {
    let args = Args::parse();

    Builder::from_default_env()
        .filter_level(LevelFilter::Info)
        .init();

    // Get working directory - use BUILD_WORKSPACE_DIRECTORY if set (running via bazel run),
    // otherwise use current directory
    let cwd = match env::var("BUILD_WORKING_DIRECTORY") {
        Ok(var) => PathBuf::from(var),
        Err(_) => env::current_dir().unwrap_or_else(|e| {
            error!("Failed to get current directory: {}", e);
            exit(1);
        }),
    };

    debug!("Working directory: {:?}", cwd);

    // If args.project is relative, make it absolute by joining to cwd
    let project_path = match args.project {
        Some(path) => {
            if path.is_relative() {
                cwd.join(&path)
            } else {
                path
            }
        }
        None => match env::var("BUILD_WORKSPACE_DIRECTORY") {
            Ok(var) => PathBuf::from(var).join("rust-project.json"),
            Err(_) => cwd.join("rust-project.json"),
        },
    };

    debug!("Project path: {:?}", project_path);

    if !project_path.exists() {
        error!("rust-project.json not found at {:?}", project_path);
        exit(1);
    }

    info!("Validating rust-project.json: {}", project_path.display());

    // Use runfiles to locate the rust-analyzer binary
    let r = Runfiles::create().unwrap_or_else(|e| {
        error!("Failed to create runfiles: {}", e);
        exit(1);
    });

    let rust_analyzer_path =
        rlocation!(r, env!("RUST_ANALYZER_RLOCATIONPATH")).unwrap_or_else(|| {
            error!("Could not locate rust-analyzer binary");
            error!("Rlocationpath: {}", env!("RUST_ANALYZER_RLOCATIONPATH"));
            exit(1);
        });

    if !rust_analyzer_path.exists() {
        error!(
            "rust-analyzer binary not found at: {}",
            rust_analyzer_path.display()
        );
        exit(1);
    }

    debug!("Using rust-analyzer: {}", rust_analyzer_path.display());

    // Run rust-analyzer diagnostics to validate the project
    let project_dir = project_path.parent().unwrap_or_else(|| {
        error!("Could not determine project directory");
        exit(1);
    });

    let mut cmd = Command::new(&rust_analyzer_path);
    cmd.arg("diagnostics")
        .arg(project_dir)
        .env("RUST_PROJECT_JSON", &project_path);

    if args.verbose {
        cmd.arg("-v");
    }

    debug!("Running: {:?}", cmd);

    let output = cmd.output().unwrap_or_else(|e| {
        error!("Failed to execute rust-analyzer: {}", e);
        exit(1);
    });

    // Check stderr for JSON deserialization errors
    let stderr = String::from_utf8_lossy(&output.stderr);

    // Look for specific JSON/deserialization errors
    if stderr.contains("Failed to deserialize")
        || stderr.contains("unknown variant")
        || stderr.contains("Failed to load the project")
    {
        error!("rust-project.json has format errors:");
        // Only show the relevant error lines
        for line in stderr.lines() {
            if line.contains("Failed to")
                || line.contains("unknown variant")
                || line.contains("ERROR")
            {
                error!("{}", line);
            }
        }
        exit(1);
    }

    // If rust-analyzer ran diagnostics (even with code errors), the JSON is valid
    // We don't care about code analysis errors, only JSON format errors
    let stdout = String::from_utf8_lossy(&output.stdout);
    if !stdout.is_empty() {
        debug!("rust-analyzer output:");
        debug!("{}", stdout);
    }

    info!("rust-project.json validation successful!");
    exit(0);
}
