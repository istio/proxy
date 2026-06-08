use std::{
    env,
    fs::OpenOptions,
    io::{BufWriter, ErrorKind},
};

use anyhow::{bail, Context};
use camino::Utf8PathBuf;
use clap::Parser;
use gen_rust_project_lib::{bazel_info, generate_rust_project};

fn write_rust_project() -> anyhow::Result<()> {
    let Config {
        workspace,
        execution_root,
        output_base,
        bazel,
        bazel_args,
        targets,
    } = Config::parse()?;

    let rules_rust_name = env!("ASPECT_REPOSITORY");

    let rust_project = generate_rust_project(
        &bazel,
        &output_base,
        &workspace,
        &execution_root,
        &[],
        &bazel_args,
        rules_rust_name,
        &targets,
    )?;

    let rust_project_path = &workspace.join("rust-project.json");

    // Try to remove the existing rust-project.json. It's OK if the file doesn't exist.
    match std::fs::remove_file(rust_project_path) {
        Ok(_) => {}
        Err(err) if err.kind() == ErrorKind::NotFound => {}
        Err(err) => bail!("Unexpected error removing old rust-project.json: {}", err),
    }

    // Write the new rust-project.json file.
    let file = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open(rust_project_path)
        .with_context(|| format!("could not open: {rust_project_path}"))
        .map(BufWriter::new)?;

    serde_json::to_writer_pretty(file, &rust_project)?;
    Ok(())
}

// TODO(david): This shells out to an expected rule in the workspace root //:rust_analyzer that the user must define.
// It would be more convenient if it could automatically discover all the rust code in the workspace if this target
// does not exist.
fn main() -> anyhow::Result<()> {
    env_logger::init();

    // Write rust-project.json.
    write_rust_project()?;
    Ok(())
}

#[derive(Debug)]
pub struct Config {
    /// The path to the Bazel workspace directory. If not specified, uses the result of `bazel info workspace`.
    workspace: Utf8PathBuf,

    /// The path to the Bazel execution root. If not specified, uses the result of `bazel info execution_root`.
    execution_root: Utf8PathBuf,

    /// The path to the Bazel output user root. If not specified, uses the result of `bazel info output_base`.
    output_base: Utf8PathBuf,

    /// The path to a Bazel binary.
    bazel: Utf8PathBuf,

    /// Arguments to pass to `bazel` invocations.
    /// See the [Command-Line Reference](<https://bazel.build/reference/command-line-reference>)
    /// for more details.
    bazel_args: Vec<String>,

    /// Space separated list of target patterns that comes after all other args.
    targets: Vec<String>,
}

impl Config {
    // Parse the configuration flags and supplement with bazel info as needed.
    pub fn parse() -> anyhow::Result<Self> {
        let ConfigParser {
            workspace,
            execution_root,
            output_base,
            bazel,
            config,
            targets,
        } = ConfigParser::parse();

        let bazel_args = config
            .into_iter()
            .map(|s| format!("--config={s}"))
            .collect();

        // Implemented this way instead of a classic `if let` to satisfy the
        // borrow checker.
        // See: <https://github.com/rust-lang/rust/issues/54663>
        #[allow(clippy::unnecessary_unwrap)]
        if workspace.is_some() && execution_root.is_some() && output_base.is_some() {
            return Ok(Config {
                workspace: workspace.unwrap(),
                execution_root: execution_root.unwrap(),
                output_base: output_base.unwrap(),
                bazel,
                bazel_args,
                targets,
            });
        }

        // We need some info from `bazel info`. Fetch it now.
        let mut info_map = bazel_info(
            &bazel,
            workspace.as_deref(),
            output_base.as_deref(),
            &[],
            &[],
        )?;

        let config = Config {
            workspace: info_map
                .remove("workspace")
                .expect("'workspace' must exist in bazel info")
                .into(),
            execution_root: info_map
                .remove("execution_root")
                .expect("'execution_root' must exist in bazel info")
                .into(),
            output_base: info_map
                .remove("output_base")
                .expect("'output_base' must exist in bazel info")
                .into(),
            bazel,
            bazel_args,
            targets,
        };

        Ok(config)
    }
}

#[derive(Debug, Parser)]
struct ConfigParser {
    /// The path to the Bazel workspace directory. If not specified, uses the result of `bazel info workspace`.
    #[clap(long, env = "BUILD_WORKSPACE_DIRECTORY")]
    workspace: Option<Utf8PathBuf>,

    /// The path to the Bazel execution root. If not specified, uses the result of `bazel info execution_root`.
    #[clap(long)]
    execution_root: Option<Utf8PathBuf>,

    /// The path to the Bazel output user root. If not specified, uses the result of `bazel info output_base`.
    #[clap(long, env = "OUTPUT_BASE")]
    output_base: Option<Utf8PathBuf>,

    /// The path to a Bazel binary.
    #[clap(long, default_value = "bazel")]
    bazel: Utf8PathBuf,

    /// A config to pass to Bazel invocations with `--config=<config>`.
    #[clap(long)]
    config: Option<String>,

    /// Space separated list of target patterns that comes after all other args.
    #[clap(default_value = "@//...")]
    targets: Vec<String>,
}
