//! The cli entrypoint for the `splice` subcommand

use std::path::PathBuf;

use anyhow::Context;
use camino::Utf8PathBuf;
use clap::Parser;

use crate::cli::Result;
use crate::config::Config;
use crate::metadata::{
    write_metadata, Cargo, CargoUpdateRequest, Generator, MetadataGenerator, TreeResolver,
};
use crate::splicing::{generate_lockfile, Splicer, SplicingManifest, WorkspaceMetadata};

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

    #[clap(long)]
    pub nonhermetic_root_bazel_workspace_dir: PathBuf,
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
    let splicer = Splicer::new(splicing_dir, splicing_manifest)?;

    let cargo = Cargo::new(opt.cargo, opt.rustc.clone());

    // Splice together the manifest
    let manifest_path = splicer
        .splice_workspace(&opt.nonhermetic_root_bazel_workspace_dir)
        .context("Failed to splice workspace")?;

    // Generate a lockfile
    let cargo_lockfile = generate_lockfile(
        &manifest_path,
        &opt.cargo_lockfile,
        cargo.clone(),
        &opt.repin,
    )
    .context("Failed to generate lockfile")?;

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

    let output_dir = opt.output_dir.clone();

    // Write metadata to the workspace for future reuse
    let (cargo_metadata, _) = Generator::new()
        .with_cargo(cargo)
        .with_rustc(opt.rustc)
        .generate(manifest_path.as_path_buf())
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

    // Generate the consumable outputs of the splicing process
    std::fs::create_dir_all(&output_dir)
        .with_context(|| format!("Failed to create directories for {}", &output_dir.display()))?;

    write_metadata(&opt.output_dir.join("metadata.json"), &cargo_metadata)
        .context("Failed to write metadata")?;

    std::fs::copy(cargo_lockfile_path, output_dir.join("Cargo.lock"))
        .context("Failed to copy lockfile")?;

    Ok(())
}
