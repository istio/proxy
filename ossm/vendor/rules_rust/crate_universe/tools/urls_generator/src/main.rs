//! A helper tool for generating urls and sha256 checksums of cargo-bazel binaries and writing them to a module.

use std::collections::BTreeMap;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::{env, fs};

use clap::Parser;
use hex::ToHex;
use sha2::{Digest, Sha256};

#[derive(Parser, Debug)]
struct Options {
    /// The path to an artifacts directory expecting to contain directories
    /// named after platform tripes with binaries inside.
    #[clap(long)]
    pub(crate) artifacts_dir: PathBuf,

    /// A url prefix where the artifacts can be found
    #[clap(long)]
    pub(crate) url_prefix: String,

    /// The path to a buildifier binary. If set, it will be ran on the module
    #[clap(long)]
    pub(crate) buildifier: Option<PathBuf>,
}

struct Artifact {
    pub(crate) url: String,
    pub(crate) triple: String,
    pub(crate) sha256: String,
}

fn calculate_sha256(file_path: &Path) -> String {
    let file = fs::File::open(file_path).unwrap();
    let mut reader = BufReader::new(file);
    let mut hasher = Sha256::new();

    loop {
        let consummed = {
            let buffer = reader.fill_buf().unwrap();
            if buffer.is_empty() {
                break;
            }
            hasher.update(buffer);
            buffer.len()
        };
        reader.consume(consummed);
    }

    let digest = hasher.finalize();
    digest.encode_hex::<String>()
}

fn locate_artifacts(artifacts_dir: &Path, url_prefix: &str) -> Vec<Artifact> {
    let artifact_dirs: Vec<PathBuf> = artifacts_dir
        .read_dir()
        .unwrap()
        .flatten()
        .filter(|entry| entry.path().is_dir())
        .map(|entry| entry.path())
        .collect();

    artifact_dirs
        .iter()
        .map(|path| {
            let triple = path.file_name().unwrap().to_string_lossy();
            let mut artifacts: Vec<Artifact> = path
                .read_dir()
                .unwrap()
                .flatten()
                .map(|f_entry| {
                    let f_path = f_entry.path();
                    let stem = f_path.file_stem().unwrap().to_string_lossy();
                    let extension = f_path
                        .extension()
                        .map(|ext| format!(".{}", ext.to_string_lossy()))
                        .unwrap_or_default();
                    Artifact {
                        url: format!("{url_prefix}/{stem}-{triple}{extension}"),
                        triple: triple.to_string(),
                        sha256: calculate_sha256(&f_entry.path()),
                    }
                })
                .collect();
            if artifacts.len() > 1 {
                panic!("Too many artifacts given for {}", triple)
            }
            artifacts.pop().unwrap()
        })
        .collect()
}

const TEMPLATE: &str = r#""""A file containing urls and associated sha256 values for cargo-bazel binaries

This file is auto-generated for each release to match the urls and sha256s of
the binaries produced for it.
"""

# Example:
# {
#     "x86_64-unknown-linux-gnu": "https://domain.com/downloads/cargo-bazel-x86_64-unknown-linux-gnu",
#     "x86_64-apple-darwin": "https://domain.com/downloads/cargo-bazel-x86_64-apple-darwin",
#     "x86_64-pc-windows-msvc": "https://domain.com/downloads/cargo-bazel-x86_64-pc-windows-msvc",
# }
CARGO_BAZEL_URLS = {}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("@cargo_bazel_bootstrap//:binary")
"#;

fn render_module(artifacts: &[Artifact]) -> String {
    let urls: BTreeMap<&String, &String> = artifacts
        .iter()
        .map(|artifact| (&artifact.triple, &artifact.url))
        .collect();

    let sha256s: BTreeMap<&String, &String> = artifacts
        .iter()
        .map(|artifact| (&artifact.triple, &artifact.sha256))
        .collect();

    TEMPLATE
        .replace(
            "CARGO_BAZEL_URLS = {}",
            &format!(
                "CARGO_BAZEL_URLS = {}",
                serde_json::to_string_pretty(&urls).unwrap()
            ),
        )
        .replace(
            "CARGO_BAZEL_SHA256S = {}",
            &format!(
                "CARGO_BAZEL_SHA256S = {}",
                serde_json::to_string_pretty(&sha256s).unwrap()
            ),
        )
        .replace(
            "CARGO_BAZEL_LABEL = Label(\"@cargo_bazel_bootstrap//:binary\")",
            "CARGO_BAZEL_LABEL = Label(\"//crate_universe:cargo_bazel_bin\")",
        )
}

fn write_module(content: &str) -> PathBuf {
    let dest = PathBuf::from(
        env::var("BUILD_WORKSPACE_DIRECTORY").expect("This binary is required to run under Bazel"),
    )
    .join(env!("MODULE_ROOT_PATH"));

    fs::write(&dest, content).unwrap();

    dest
}

fn run_buildifier(buildifier_path: &Path, module: &Path) {
    Command::new(buildifier_path)
        .arg("-lint=fix")
        .arg("-mode=fix")
        .arg("-warnings=all")
        .arg(module)
        .output()
        .unwrap();
}

fn main() {
    let opt = Options::parse();

    let artifacts = locate_artifacts(&opt.artifacts_dir, &opt.url_prefix);

    let content = render_module(&artifacts);

    let path = write_module(&content);

    if let Some(buildifier_path) = opt.buildifier {
        run_buildifier(&buildifier_path, &path);
    }
}
