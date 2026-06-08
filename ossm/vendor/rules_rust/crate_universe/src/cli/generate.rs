//! The cli entrypoint for the `generate` subcommand

use std::collections::BTreeSet;
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

    /// Whether to skip writing the cargo lockfile back after resolving.
    /// You may want to set this if your dependency versions are maintained externally through a non-trivial set-up.
    /// But you probably don't want to set this.
    #[clap(long)]
    pub skip_cargo_lockfile_overwrite: bool,

    /// Whether to strip internal dependencies from the cargo lockfile.
    /// You may want to use this if you want to maintain a cargo lockfile for bazel only.
    /// Bazel only requires external dependencies to be present in the lockfile.
    /// By removing internal dependencies, the lockfile changes less frequently which reduces merge conflicts
    /// in other lockfiles where the cargo lockfile's sha is stored.
    #[clap(long)]
    pub strip_internal_dependencies_from_cargo_lockfile: bool,
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

            let splicing_manifest = SplicingManifest::try_from_path(&opt.splicing_manifest)?;

            write_paths_to_track(
                &opt.paths_to_track,
                &opt.warnings_output_path,
                splicing_manifest.manifests.keys().cloned(),
                context
                    .crates
                    .values()
                    .filter_map(|crate_context| crate_context.repository.as_ref()),
                context.unused_patches.iter(),
                &opt.nonhermetic_root_bazel_workspace_dir,
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
    let lockfile_path = metadata_path
        .parent()
        .expect("metadata files should always have parents")
        .join("Cargo.lock");
    if !lockfile_path.exists() {
        bail!(
            "The metadata file at {} is not next to a `Cargo.lock` file.",
            metadata_path.display()
        )
    }
    let (cargo_metadata, cargo_lockfile) = load_metadata(metadata_path, &lockfile_path)?;

    // Annotate metadata
    let annotations = Annotations::new(
        cargo_metadata,
        &Some(lockfile_path),
        cargo_lockfile.clone(),
        config.clone(),
        &opt.nonhermetic_root_bazel_workspace_dir,
    )?;

    let splicing_manifest = SplicingManifest::try_from_path(&opt.splicing_manifest)?;

    write_paths_to_track(
        &opt.paths_to_track,
        &opt.warnings_output_path,
        splicing_manifest.manifests.keys().cloned(),
        annotations.lockfile.crates.values(),
        cargo_lockfile.patch.unused.iter(),
        &opt.nonhermetic_root_bazel_workspace_dir,
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
        let lock_content =
            lock_context(context, &config, &splicing_manifest, &cargo_bin, rustc_bin)?;

        write_lockfile(lock_content, &lockfile, opt.dry_run)?;
    }

    if !opt.skip_cargo_lockfile_overwrite {
        let cargo_lockfile_to_write = if opt.strip_internal_dependencies_from_cargo_lockfile {
            remove_internal_dependencies_from_cargo_lockfile(cargo_lockfile)
        } else {
            cargo_lockfile
        };
        update_cargo_lockfile(&opt.cargo_lockfile, cargo_lockfile_to_write)?;
    }

    Ok(())
}

fn remove_internal_dependencies_from_cargo_lockfile(cargo_lockfile: Lockfile) -> Lockfile {
    let filtered_packages: Vec<_> = cargo_lockfile
        .packages
        .into_iter()
        // Filter packages to only keep external dependencies (those with a source)
        .filter(|pkg| pkg.source.is_some())
        .collect();

    Lockfile {
        packages: filtered_packages,
        ..cargo_lockfile
    }
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
    Paths: Iterator<Item = Utf8PathBuf>,
    UnusedPatches: Iterator<Item = &'a cargo_lock::Dependency>,
>(
    output_file: &Path,
    warnings_output_path: &Path,
    manifests: Paths,
    source_annotations: SourceAnnotations,
    unused_patches: UnusedPatches,
    nonhermetic_root_bazel_workspace_dir: &Utf8PathBuf,
) -> Result<()> {
    let source_annotation_manifests: BTreeSet<_> = source_annotations
        .filter_map(|v| {
            if let SourceAnnotation::Path { path } = v {
                Some(path.join("Cargo.toml"))
            } else {
                None
            }
        })
        .collect();
    let paths_to_track: BTreeSet<_> = source_annotation_manifests
        .iter()
        .cloned()
        .chain(manifests)
        // Paths outside the bazel workspace cannot be `.watch`-ed.
        .filter(|p| p.starts_with(nonhermetic_root_bazel_workspace_dir))
        .collect();
    std::fs::write(
        output_file,
        serde_json::to_string(&paths_to_track).context("Failed to serialize paths to track")?,
    )
    .context("Failed to write paths to track")?;

    let mut warnings = Vec::new();
    for source_annotation_manifest in &source_annotation_manifests {
        warnings.push(format!("Build is not hermetic - path dependency pulling in crate at {source_annotation_manifest} is being used."));
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test;

    #[test]
    fn test_remove_internal_dependencies_from_cargo_lockfile_workspace_build_scripts_deps_should_remove_internal_dependencies(
    ) {
        let original_lockfile = test::lockfile::workspace_build_scripts_deps();

        let filtered_lockfile =
            remove_internal_dependencies_from_cargo_lockfile(original_lockfile.clone());

        assert!(filtered_lockfile.packages.len() < original_lockfile.packages.len());

        assert!(original_lockfile
            .packages
            .iter()
            .any(|pkg| pkg.name.as_str() == "child"));
        assert!(!filtered_lockfile
            .packages
            .iter()
            .any(|pkg| pkg.name.as_str() == "child"));

        assert!(filtered_lockfile
            .packages
            .iter()
            .any(|pkg| pkg.name.as_str() == "anyhow"));
    }
}
