use crate::context::SingleBuildFileRenderContext;
use crate::rendering::Renderer;

use anyhow::{Context, Result};
use clap::Parser;

use std::path::PathBuf;
use std::sync::Arc;

#[derive(Parser, Debug)]
#[clap(about = "Command line options for the `render` subcommand", version)]
pub struct RenderOptions {
    #[clap(long)]
    options_json: String,

    #[clap(long)]
    output_path: PathBuf,
}

pub fn render(opt: RenderOptions) -> Result<()> {
    let RenderOptions {
        options_json,
        output_path,
    } = opt;

    let deserialized_options = serde_json::from_str(&options_json)
        .with_context(|| format!("Failed to deserialize options_json from '{}'", options_json))?;

    let SingleBuildFileRenderContext {
        config,
        supported_platform_triples,
        platform_conditions,
        crate_context,
    } = deserialized_options;

    let renderer = Renderer::new(config, supported_platform_triples);
    let platforms = renderer.render_platform_labels(Arc::clone(&platform_conditions));
    let engine = renderer.create_engine(platform_conditions);
    let output = renderer
        .render_one_build_file(&engine, &platforms, &crate_context)
        .with_context(|| {
            format!(
                "Failed to render BUILD.bazel file for crate {}",
                crate_context.name
            )
        })?;
    std::fs::write(&output_path, output.as_bytes()).with_context(|| {
        format!(
            "Failed to write BUILD.bazel file to {}",
            output_path.display()
        )
    })?;

    Ok(())
}
