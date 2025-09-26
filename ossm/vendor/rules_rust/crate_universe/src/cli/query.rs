//! The cli entrypoint for the `query` subcommand

use std::fs;
use std::path::PathBuf;

use anyhow::{bail, Result};
use clap::Parser;

use crate::config::Config;
use crate::context::Context;
use crate::lockfile::Digest;
use crate::metadata::Cargo;
use crate::splicing::SplicingManifest;

/// Command line options for the `query` subcommand
#[derive(Parser, Debug)]
#[clap(about = "Command line options for the `query` subcommand", version)]
pub struct QueryOptions {
    /// The lockfile path for reproducible Cargo->Bazel renderings
    #[clap(long)]
    pub lockfile: PathBuf,

    /// The config file with information about the Bazel and Cargo workspace
    #[clap(long)]
    pub config: PathBuf,

    /// A generated manifest of splicing inputs
    #[clap(long)]
    pub splicing_manifest: PathBuf,

    /// The path to a Cargo binary to use for gathering metadata
    #[clap(long, env = "CARGO")]
    pub cargo: PathBuf,

    /// The path to a rustc binary for use with Cargo
    #[clap(long, env = "RUSTC")]
    pub rustc: PathBuf,
}

/// Determine if the current lockfile needs to be re-pinned
pub fn query(opt: QueryOptions) -> Result<()> {
    // Read the lockfile
    let content = match fs::read_to_string(&opt.lockfile) {
        Ok(c) => c,
        Err(_) => bail!("Unable to read lockfile"),
    };

    // Deserialize it so we can easily compare it with
    let lockfile: Context = match serde_json::from_str(&content) {
        Ok(ctx) => ctx,
        Err(_) => bail!("Could not load lockfile"),
    };

    // Check to see if a digest has been set
    let digest = match &lockfile.checksum {
        Some(d) => d.clone(),
        None => bail!("No digest provided in lockfile"),
    };

    // Load the config file
    let config = Config::try_from_path(&opt.config)?;

    let splicing_manifest = SplicingManifest::try_from_path(&opt.splicing_manifest)?;

    // Generate a new digest so we can compare it with the one in the lockfile
    let expected = Digest::new(
        &lockfile,
        &config,
        &splicing_manifest,
        &Cargo::new(opt.cargo, opt.rustc.clone()),
        &opt.rustc,
    )?;

    if digest != expected {
        bail!("Digests do not match: Current {digest:?} != Expected {expected:?}");
    }

    // There is no need to repin
    Ok(())
}
