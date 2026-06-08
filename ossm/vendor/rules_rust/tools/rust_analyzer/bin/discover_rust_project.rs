//! Binary used for automatic Rust workspace discovery by `rust-analyzer`.
//! See [rust-analyzer documentation][rd] for a thorough description of this interface.
//! [rd]: <https://rust-analyzer.github.io/manual.html#rust-analyzer.workspace.discoverConfig>.

use std::{
    env,
    io::{self, Write},
};

use anyhow::Context;
use camino::{Utf8Path, Utf8PathBuf};
use clap::Parser;
use env_logger::{fmt::Formatter, Target, WriteStyle};
use gen_rust_project_lib::{
    bazel_info, generate_rust_project, DiscoverProject, RustAnalyzerArg, BUILD_FILE_NAMES,
    WORKSPACE_ROOT_FILE_NAMES,
};
use log::{LevelFilter, Record};

/// Looks within the current directory for a file that marks a bazel workspace.
///
/// # Errors
///
/// Returns an error if no file from [`WORKSPACE_ROOT_FILE_NAMES`] is found.
fn find_workspace_root_file(workspace: &Utf8Path) -> anyhow::Result<Utf8PathBuf> {
    BUILD_FILE_NAMES
        .iter()
        .chain(WORKSPACE_ROOT_FILE_NAMES)
        .map(|file| workspace.join(file))
        .find(|p| p.exists())
        .with_context(|| format!("no root file found for bazel workspace {workspace}"))
}

fn project_discovery() -> anyhow::Result<DiscoverProject<'static>> {
    let Config {
        workspace,
        execution_root,
        output_base,
        bazel,
        bazel_startup_options,
        bazel_args,
        rust_analyzer_argument,
    } = Config::parse()?;

    log::info!("got rust-analyzer argument: {rust_analyzer_argument:?}");

    let ra_arg = match rust_analyzer_argument {
        Some(ra_arg) => ra_arg,
        None => RustAnalyzerArg::Buildfile(find_workspace_root_file(&workspace)?),
    };

    let rules_rust_name = env!("ASPECT_REPOSITORY");

    log::info!("resolved rust-analyzer argument: {ra_arg:?}");

    let (buildfile, targets) = ra_arg.into_target_details(&workspace)?;

    log::debug!("got buildfile: {buildfile}");
    log::debug!("got targets: {targets}");

    // Use the generated files to print the rust-project.json.
    let project = generate_rust_project(
        &bazel,
        &output_base,
        &workspace,
        &execution_root,
        &bazel_startup_options,
        &bazel_args,
        rules_rust_name,
        &[targets],
    )?;

    Ok(DiscoverProject::Finished { buildfile, project })
}

#[allow(clippy::writeln_empty_string)]
fn write_discovery<W>(mut writer: W, discovery: DiscoverProject) -> std::io::Result<()>
where
    W: Write,
{
    serde_json::to_writer(&mut writer, &discovery)?;
    // `rust-analyzer` reads messages line by line, so we must add a newline after each
    writeln!(writer, "")
}

fn main() -> anyhow::Result<()> {
    let log_format_fn = |fmt: &mut Formatter, rec: &Record| {
        let message = rec.args();
        let discovery = DiscoverProject::Progress { message };
        write_discovery(fmt, discovery)
    };

    // Treat logs as progress messages.
    env_logger::Builder::from_default_env()
        // Never write color/styling info
        .write_style(WriteStyle::Never)
        // Format logs as progress messages
        .format(log_format_fn)
        // `rust-analyzer` reads the stdout
        .filter_level(LevelFilter::Debug)
        .target(Target::Stdout)
        .init();

    let discovery = match project_discovery() {
        Ok(discovery) => discovery,
        Err(error) => DiscoverProject::Error {
            error: error.to_string(),
            source: error.source().as_ref().map(ToString::to_string),
        },
    };

    write_discovery(io::stdout(), discovery)?;
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

    /// Startup options to pass to `bazel` invocations.
    /// See the [Command-Line Reference](<https://bazel.build/reference/command-line-reference>)
    /// for more details.
    bazel_startup_options: Vec<String>,

    /// Arguments to pass to `bazel` invocations.
    /// See the [Command-Line Reference](<https://bazel.build/reference/command-line-reference>)
    /// for more details.
    bazel_args: Vec<String>,

    /// The argument that `rust-analyzer` can pass to the binary.
    rust_analyzer_argument: Option<RustAnalyzerArg>,
}

impl Config {
    // Parse the configuration flags and supplement with bazel info as needed.
    pub fn parse() -> anyhow::Result<Self> {
        let ConfigParser {
            workspace,
            bazel,
            bazel_startup_options,
            bazel_args,
            rust_analyzer_argument,
        } = ConfigParser::parse();

        // We need some info from `bazel info`. Fetch it now.
        let mut info_map = bazel_info(
            &bazel,
            workspace.as_deref(),
            None,
            &bazel_startup_options,
            &bazel_args,
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
            bazel_startup_options,
            bazel_args,
            rust_analyzer_argument,
        };

        Ok(config)
    }
}

#[derive(Debug, Parser)]
struct ConfigParser {
    /// The path to the Bazel workspace directory. If not specified, uses the result of `bazel info workspace`.
    #[clap(long, env = "BUILD_WORKSPACE_DIRECTORY")]
    workspace: Option<Utf8PathBuf>,

    /// The path to a Bazel binary.
    #[clap(long, default_value = "bazel")]
    bazel: Utf8PathBuf,

    /// Startup options to pass to `bazel` invocations.
    /// See the [Command-Line Reference](<https://bazel.build/reference/command-line-reference>)
    /// for more details.
    #[clap(long = "bazel_startup_option")]
    bazel_startup_options: Vec<String>,

    /// Arguments to pass to `bazel` invocations.
    /// See the [Command-Line Reference](<https://bazel.build/reference/command-line-reference>)
    /// for more details.
    #[clap(long = "bazel_arg")]
    bazel_args: Vec<String>,

    /// The argument that `rust-analyzer` can pass to the binary.
    rust_analyzer_argument: Option<RustAnalyzerArg>,
}
