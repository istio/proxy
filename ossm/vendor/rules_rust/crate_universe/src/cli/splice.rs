//! The cli entrypoint for the `splice` subcommand

use std::fs::File;
use std::path::PathBuf;
use std::process::Stdio;

use anyhow::Context;
use camino::Utf8PathBuf;
use clap::Parser;
use itertools::Itertools;

use crate::cli::Result;
use crate::config::Config;
use crate::metadata::{Cargo, CargoUpdateRequest, TreeResolver};
use crate::splicing::{
    generate_lockfile, Splicer, SplicerKind, SplicingManifest, WorkspaceMetadata,
};

/// Command line options for the `splice` subcommand
#[derive(Parser, Debug)]
#[clap(about = "Command line options for the `splice` subcommand", version)]
pub struct SpliceOptions {
    /// A generated manifest of splicing inputs
    #[clap(long)]
    pub splicing_manifest: PathBuf,

    /// The path to a [Cargo.lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html) file.
    #[clap(long)]
    pub cargo_lockfile: Option<PathBuf>,

    /// The desired update/repin behavior
    #[clap(long, env = "CARGO_BAZEL_REPIN", num_args=0..=1, default_missing_value = "true")]
    pub repin: Option<CargoUpdateRequest>,

    /// The directory in which to build the workspace. If this argument is not
    /// passed, a temporary directory will be generated.
    #[clap(long)]
    pub workspace_dir: Option<Utf8PathBuf>,

    /// The location where the results of splicing are written.
    #[clap(long)]
    pub output_dir: PathBuf,

    /// If true, outputs will be printed instead of written to disk.
    #[clap(long)]
    pub dry_run: bool,

    /// The path to a Cargo configuration file.
    #[clap(long)]
    pub cargo_config: Option<PathBuf>,

    /// The path to the config file (containing [crate::config::Config].)
    #[clap(long)]
    pub config: PathBuf,

    /// The path to a Cargo binary to use for gathering metadata
    #[clap(long, env = "CARGO")]
    pub cargo: PathBuf,

    /// The path to a rustc binary for use with Cargo
    #[clap(long, env = "RUSTC")]
    pub rustc: PathBuf,

    /// The name of the repository being generated.
    #[clap(long)]
    pub repository_name: String,

    /// Whether to skip writing the cargo lockfile back after resolving.
    /// You may want to set this if your dependency versions are maintained externally through a non-trivial set-up.
    /// But you probably don't want to set this.
    #[clap(long)]
    pub skip_cargo_lockfile_overwrite: bool,

    /// The path to the Bazel root workspace (i.e. the directory containing the WORKSPACE.bazel file or similar).
    /// BE CAREFUL with this value. We never want to include it in a lockfile hash (to keep lockfiles portable),
    /// which means you also should not use it anywhere that _should_ be guarded by a lockfile hash.
    /// You basically never want to use this value.
    #[clap(long)]
    pub nonhermetic_root_bazel_workspace_dir: Utf8PathBuf,
}

/// Combine a set of disjoint manifests into a single workspace.
pub fn splice(opt: SpliceOptions) -> Result<()> {
    // Load the all config files required for splicing a workspace
    let splicing_manifest = SplicingManifest::try_from_path(&opt.splicing_manifest)
        .context("Failed to parse splicing manifest")?;

    // Determine the splicing workspace
    let temp_dir;
    let splicing_dir = match &opt.workspace_dir {
        Some(dir) => dir.clone(),
        None => {
            temp_dir = tempfile::tempdir().context("Failed to generate temporary directory")?;
            Utf8PathBuf::from_path_buf(temp_dir.as_ref().to_path_buf())
                .unwrap_or_else(|path| panic!("Temporary directory wasn't valid UTF-8: {:?}", path))
        }
    };

    // Generate a splicer for creating a Cargo workspace manifest
    let splicer = Splicer::new(splicing_dir.clone(), splicing_manifest)?;
    let prepared_splicer = splicer.prepare()?;

    let cargo = Cargo::new(opt.cargo, opt.rustc.clone());

    // Splice together the manifest
    let manifest_path = prepared_splicer
        .splice(&splicing_dir, &opt.nonhermetic_root_bazel_workspace_dir)
        .with_context(|| format!("Failed to splice workspace {}", opt.repository_name))?;

    // Use the existing lockfile if possible, otherwise generate a new one.
    let cargo_lockfile = if let Some(cargo_lockfile_path) = opt
        .cargo_lockfile
        .as_ref()
        .filter(|_| opt.skip_cargo_lockfile_overwrite)
    {
        cargo_lock::Lockfile::load(cargo_lockfile_path).context(format!(
            "Failed to load lockfile: {}",
            cargo_lockfile_path.display()
        ))?
    } else {
        generate_lockfile(
            &manifest_path,
            &opt.cargo_lockfile,
            cargo.clone(),
            &opt.repin,
        )
        .context("Failed to generate lockfile")?
    };

    let config = Config::try_from_path(&opt.config).context("Failed to parse config")?;

    let resolver_data = TreeResolver::new(cargo.clone())
        .generate(
            manifest_path.as_path_buf(),
            &config.supported_platform_triples,
        )
        .context("Failed to generate features")?;

    // Write the registry url info to the manifest now that a lockfile has been generated
    WorkspaceMetadata::write_registry_urls_and_feature_map(
        &cargo,
        &cargo_lockfile,
        resolver_data,
        manifest_path.as_path_buf(),
        manifest_path.as_path_buf(),
    )
    .context("Failed to write registry URLs and feature map")?;

    // Generate the consumable outputs of the splicing process
    std::fs::create_dir_all(&opt.output_dir).with_context(|| {
        format!(
            "Failed to create directories for {}",
            opt.output_dir.display()
        )
    })?;

    let metadata_json = File::create(opt.output_dir.join("metadata.json"))?;

    // Write metadata to the workspace for future reuse
    cargo
        .metadata_command_with_options(
            manifest_path.as_path_buf().as_ref(),
            vec!["--locked".to_owned()],
        )?
        .cargo_command()
        .stdout(Stdio::from(metadata_json))
        .stderr(Stdio::null())
        .status()
        .context("Failed to generate cargo metadata")?;

    let cargo_lockfile_path = manifest_path
        .as_path_buf()
        .parent()
        .with_context(|| {
            format!(
                "The path {} is expected to have a parent directory",
                manifest_path.as_path_buf()
            )
        })?
        .join("Cargo.lock");

    std::fs::copy(cargo_lockfile_path, opt.output_dir.join("Cargo.lock"))
        .context("Failed to copy lockfile")?;

    if let SplicerKind::Workspace { path, .. } = prepared_splicer {
        let metadata = cargo.metadata_command_with_options(
            path.as_std_path(),
            vec![String::from("--no-deps")],
        )?.exec().with_context(|| {
                format!(
                    "Error spawning cargo in child process to compute crate paths for workspace '{}'",
                    path
                )
            })?;
        let contents = metadata
            .packages
            .into_iter()
            .map(|package| package.manifest_path)
            .join("\n");
        std::fs::write(opt.output_dir.join("extra_paths_to_track"), contents)?;
    }
    Ok(())
}
