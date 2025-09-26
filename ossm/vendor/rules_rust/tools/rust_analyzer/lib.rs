use std::collections::HashMap;
use std::path::Path;
use std::process::Command;

use anyhow::anyhow;
use runfiles::Runfiles;

mod aquery;
mod rust_project;

pub fn generate_crate_info(
    bazel: impl AsRef<Path>,
    workspace: impl AsRef<Path>,
    rules_rust: impl AsRef<str>,
    targets: &[String],
) -> anyhow::Result<()> {
    log::debug!("Building rust_analyzer_crate_spec files for {:?}", targets);

    let output = Command::new(bazel.as_ref())
        .current_dir(workspace.as_ref())
        .env_remove("BAZELISK_SKIP_WRAPPER")
        .env_remove("BUILD_WORKING_DIRECTORY")
        .env_remove("BUILD_WORKSPACE_DIRECTORY")
        .arg("build")
        .arg("--norun_validations")
        .arg(format!(
            "--aspects={}//rust:defs.bzl%rust_analyzer_aspect",
            rules_rust.as_ref()
        ))
        .arg("--output_groups=rust_analyzer_crate_spec,rust_generated_srcs")
        .args(targets)
        .output()?;

    if !output.status.success() {
        return Err(anyhow!(
            "bazel build failed:({})\n{}",
            output.status,
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    Ok(())
}

pub fn write_rust_project(
    bazel: impl AsRef<Path>,
    workspace: impl AsRef<Path>,
    rules_rust_name: &impl AsRef<str>,
    targets: &[String],
    execution_root: impl AsRef<Path>,
    output_base: impl AsRef<Path>,
    rust_project_path: impl AsRef<Path>,
) -> anyhow::Result<()> {
    let crate_specs = aquery::get_crate_specs(
        bazel.as_ref(),
        workspace.as_ref(),
        execution_root.as_ref(),
        targets,
        rules_rust_name.as_ref(),
    )?;

    let path = runfiles::rlocation!(
        Runfiles::create()?,
        "rules_rust/rust/private/rust_analyzer_detect_sysroot.rust_analyzer_toolchain.json"
    )
    .unwrap();
    let toolchain_info: HashMap<String, String> =
        serde_json::from_str(&std::fs::read_to_string(path)?)?;

    let sysroot_src = &toolchain_info["sysroot_src"];
    let sysroot = &toolchain_info["sysroot"];

    let rust_project = rust_project::generate_rust_project(sysroot, sysroot_src, &crate_specs)?;

    rust_project::write_rust_project(
        rust_project_path.as_ref(),
        workspace.as_ref(),
        execution_root.as_ref(),
        output_base.as_ref(),
        &rust_project,
    )?;

    Ok(())
}
