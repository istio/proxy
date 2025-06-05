//! The cli entrypoint for the `generate` subcommand

use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use anyhow::{bail, Context as AnyhowContext, Result};
use camino::Utf8PathBuf;
use cargo_lock::Lockfile;
use clap::Parser;

use crate::config::Config;
use crate::context::Context;
use crate::lockfile::{lock_context, write_lockfile};
use crate::metadata::{load_metadata, Annotations, Cargo, SourceAnnotation};
use crate::rendering::{write_outputs, Renderer};
use crate::splicing::SplicingManifest;
use crate::utils::normalize_cargo_file_paths;
use crate::utils::starlark::Label;

/// Command line options for the `generate` subcommand
#[derive(Parser, Debug)]
#[clap(about = "Command line options for the `generate` subcommand", version)]
pub struct GenerateOptions {
    /// The path to a Cargo binary to use for gathering metadata
    #[clap(long, env = "CARGO")]
    pub cargo: Option<PathBuf>,

    /// The path to a rustc binary for use with Cargo
    #[clap(long, env = "RUSTC")]
    pub rustc: Option<PathBuf>,

    /// The config file with information about the Bazel and Cargo workspace
    #[clap(long)]
    pub config: PathBuf,

    /// A generated manifest of splicing inputs
    #[clap(long)]
    pub splicing_manifest: PathBuf,

    /// The path to either a Cargo or Bazel lockfile
    #[clap(long)]
    pub lockfile: Option<PathBuf>,

    /// The path to a [Cargo.lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html) file.
    #[clap(long)]
    pub cargo_lockfile: PathBuf,

    /// The directory of the current repository rule
    #[clap(long)]
    pub repository_dir: PathBuf,

    /// A [Cargo config](https://doc.rust-lang.org/cargo/reference/config.html#configuration)
    /// file to use when gathering metadata
    #[clap(long)]
    pub cargo_config: Option<PathBuf>,

    /// Whether or not to ignore the provided lockfile and re-generate one
    #[clap(long)]
    pub repin: bool,

    /// The path to a Cargo metadata `json` file. This file must be next to a `Cargo.toml` and `Cargo.lock` file.
    #[clap(long)]
    pub metadata: Option<PathBuf>,

    /// If true, outputs will be printed instead of written to disk.
    #[clap(long)]
    pub dry_run: bool,

    /// The path to the Bazel root workspace (i.e. the directory containing the WORKSPACE.bazel file or similar).
    /// BE CAREFUL with this value. We never want to include it in a lockfile hash (to keep lockfiles portable),
    /// which means you also should not use it anywhere that _should_ be guarded by a lockfile hash.
    /// You basically never want to use this value.
    #[clap(long)]
    pub nonhermetic_root_bazel_workspace_dir: Utf8PathBuf,

    /// Path to write a list of files which the repository_rule should watch.
    /// If any of these paths change, the repository rule should be rerun.
    /// These files may be outside of the Bazel-managed workspace.
    /// A (possibly empty) JSON sorted array of strings will be unconditionally written to this file.
    #[clap(long)]
    pub paths_to_track: PathBuf,

    /// The label of this binary, if it was built in bootstrap mode.
    /// BE CAREFUL with this value. We never want to include it in a lockfile hash (to keep lockfiles portable),
    /// which means you also should not use it anywhere that _should_ be guarded by a lockfile hash.
    /// You basically never want to use this value.
    #[clap(long)]
    pub(crate) generator: Option<Label>,

    /// Path to write a list of warnings which the repository rule should emit.
    /// A (possibly empty) JSON array of strings will be unconditionally written to this file.
    /// Each warning should be printed.
    /// This mechanism exists because this process's output is often hidden by default,
    /// so this provides a way for the repository rule to force printing.
    #[clap(long)]
    pub warnings_output_path: PathBuf,
}

pub fn generate(opt: GenerateOptions) -> Result<()> {
    // Load the config
    let config = Config::try_from_path(&opt.config)?;

    // Go straight to rendering if there is no need to repin
    if !opt.repin {
        if let Some(lockfile) = &opt.lockfile {
            let context = Context::try_from_path(lockfile)?;

            // Render build files
            let outputs = Renderer::new(
                Arc::new(config.rendering),
                Arc::new(config.supported_platform_triples),
            )
            .render(&context, opt.generator)?;

            // make file paths compatible with bazel labels
            let normalized_outputs = normalize_cargo_file_paths(outputs, &opt.repository_dir);

            // Write the outputs to disk
            write_outputs(normalized_outputs, opt.dry_run)?;

            write_paths_to_track(
                &opt.paths_to_track,
                &opt.warnings_output_path,
                context
                    .crates
                    .values()
                    .filter_map(|crate_context| crate_context.repository.as_ref()),
                context.unused_patches.iter(),
            )?;

            return Ok(());
        }
    }

    // Ensure Cargo and Rustc are available for use during generation.
    let rustc_bin = match &opt.rustc {
        Some(bin) => bin,
        None => bail!("The `--rustc` argument is required when generating unpinned content"),
    };

    let cargo_bin = Cargo::new(
        match opt.cargo {
            Some(bin) => bin,
            None => bail!("The `--cargo` argument is required when generating unpinned content"),
        },
        rustc_bin.clone(),
    );

    // Ensure a path to a metadata file was provided
    let metadata_path = match &opt.metadata {
        Some(path) => path,
        None => bail!("The `--metadata` argument is required when generating unpinned content"),
    };

    // Load Metadata and Lockfile
    let (cargo_metadata, cargo_lockfile) = load_metadata(metadata_path)?;

    // Annotate metadata
    let annotations = Annotations::new(
        cargo_metadata,
        cargo_lockfile.clone(),
        config.clone(),
        &opt.nonhermetic_root_bazel_workspace_dir,
    )?;

    write_paths_to_track(
        &opt.paths_to_track,
        &opt.warnings_output_path,
        annotations.lockfile.crates.values(),
        cargo_lockfile.patch.unused.iter(),
    )?;

    // Generate renderable contexts for each package
    let context = Context::new(annotations, config.rendering.are_sources_present())?;

    // Render build files
    let outputs = Renderer::new(
        Arc::new(config.rendering.clone()),
        Arc::new(config.supported_platform_triples.clone()),
    )
    .render(&context, opt.generator)?;

    // make file paths compatible with bazel labels
    let normalized_outputs = normalize_cargo_file_paths(outputs, &opt.repository_dir);

    // Write the outputs to disk
    write_outputs(normalized_outputs, opt.dry_run)?;

    // Ensure Bazel lockfiles are written to disk so future generations can be short-circuited.
    if let Some(lockfile) = opt.lockfile {
        let splicing_manifest = SplicingManifest::try_from_path(&opt.splicing_manifest)?;

        let lock_content =
            lock_context(context, &config, &splicing_manifest, &cargo_bin, rustc_bin)?;

        write_lockfile(lock_content, &lockfile, opt.dry_run)?;
    }

    update_cargo_lockfile(&opt.cargo_lockfile, cargo_lockfile)?;

    Ok(())
}

fn update_cargo_lockfile(path: &Path, cargo_lockfile: Lockfile) -> Result<()> {
    let old_contents = fs::read_to_string(path).ok();
    let new_contents = cargo_lockfile.to_string();

    // Don't overwrite identical contents because timestamp changes may invalidate repo rules.
    if old_contents.as_ref() == Some(&new_contents) {
        return Ok(());
    }

    fs::write(path, new_contents)
        .context("Failed to write Cargo.lock file back to the workspace.")?;

    Ok(())
}

fn write_paths_to_track<
    'a,
    SourceAnnotations: Iterator<Item = &'a SourceAnnotation>,
    UnusedPatches: Iterator<Item = &'a cargo_lock::Dependency>,
>(
    output_file: &Path,
    warnings_output_path: &Path,
    source_annotations: SourceAnnotations,
    unused_patches: UnusedPatches,
) -> Result<()> {
    let paths_to_track: std::collections::BTreeSet<_> = source_annotations
        .filter_map(|v| {
            if let SourceAnnotation::Path { path } = v {
                Some(path.join("Cargo.toml"))
            } else {
                None
            }
        })
        .collect();
    std::fs::write(
        output_file,
        serde_json::to_string(&paths_to_track).context("Failed to serialize paths to track")?,
    )
    .context("Failed to write paths to track")?;

    let mut warnings = Vec::new();
    for path_to_track in &paths_to_track {
        warnings.push(format!("Build is not hermetic - path dependency pulling in crate at {path_to_track} is being used."));
    }
    for unused_patch in unused_patches {
        warnings.push(format!("You have a [patch] Cargo.toml entry that is being ignored by cargo. Unused patch: {} {}{}", unused_patch.name, unused_patch.version, if let Some(source) = unused_patch.source.as_ref() { format!(" ({})", source) } else { String::new() }));
    }

    std::fs::write(
        warnings_output_path,
        serde_json::to_string(&warnings).context("Failed to serialize warnings to track")?,
    )
    .context("Failed to write warnings file")?;
    Ok(())
}
