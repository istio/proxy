//! Tools for producing Crate metadata using `cargo tree`.

use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::io::BufRead;
use std::path::{Path, PathBuf};
use std::process::Child;

use anyhow::{anyhow, bail, Context, Result};
use camino::Utf8Path;
use semver::Version;
use serde::{Deserialize, Serialize};
use tracing::debug;
use url::Url;

use crate::config::CrateId;
use crate::metadata::cargo_bin::Cargo;
use crate::select::{Select, SelectableScalar};
use crate::utils::symlink::symlink;
use crate::utils::target_triple::TargetTriple;

/// A list platform triples that support host tools
///
/// [Tier 1](https://doc.rust-lang.org/nightly/rustc/platform-support.html#tier-1-with-host-tools)
/// [Tier 2](https://doc.rust-lang.org/nightly/rustc/platform-support.html#tier-2-with-host-tools)
const RUSTC_TRIPLES_WITH_HOST_TOOLS: [&str; 26] = [
    // Tier 1
    "aarch64-apple-darwin",
    "aarch64-unknown-linux-gnu",
    "i686-pc-windows-gnu",
    "i686-pc-windows-msvc",
    "i686-unknown-linux-gnu",
    "x86_64-apple-darwin",
    "x86_64-pc-windows-gnu",
    "x86_64-pc-windows-msvc",
    "x86_64-unknown-linux-gnu",
    // Tier 2
    "aarch64-pc-windows-msvc",
    "aarch64-unknown-linux-musl",
    "arm-unknown-linux-gnueabi",
    "arm-unknown-linux-gnueabihf",
    "armv7-unknown-linux-gnueabihf",
    "loongarch64-unknown-linux-gnu",
    "loongarch64-unknown-linux-musl",
    "powerpc-unknown-linux-gnu",
    "powerpc64-unknown-linux-gnu",
    "powerpc64le-unknown-linux-gnu",
    "riscv64gc-unknown-linux-gnu",
    "riscv64gc-unknown-linux-musl",
    "s390x-unknown-linux-gnu",
    "x86_64-unknown-freebsd",
    "x86_64-unknown-illumo",
    "x86_64-unknown-linux-musl",
    "x86_64-unknown-netbsd",
];

/// Feature resolver info about a given crate.
#[derive(Debug, Default, Clone, Eq, PartialEq, Serialize, Deserialize)]
pub(crate) struct CargoTreeEntry {
    /// The set of features active on a given crate.
    #[serde(default, skip_serializing_if = "BTreeSet::is_empty")]
    pub features: BTreeSet<String>,

    /// The dependencies of a given crate based on feature resolution.
    #[serde(default, skip_serializing_if = "BTreeSet::is_empty")]
    pub deps: BTreeSet<CrateId>,
}

impl CargoTreeEntry {
    pub fn new() -> Self {
        Self {
            features: BTreeSet::new(),
            deps: BTreeSet::new(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.features.is_empty() && self.deps.is_empty()
    }

    pub fn consume(&mut self, other: Self) {
        self.features.extend(other.features);
        self.deps.extend(other.deps);
    }
}

impl SelectableScalar for CargoTreeEntry {}

/// Feature and dependency metadata generated from [TreeResolver].
pub(crate) type TreeResolverMetadata = BTreeMap<CrateId, Select<CargoTreeEntry>>;

/// Generates metadata about a Cargo workspace tree which supplements the inaccuracies in
/// standard [Cargo metadata](https://doc.rust-lang.org/cargo/commands/cargo-metadata.html)
/// due lack of [Feature resolver 2](https://doc.rust-lang.org/cargo/reference/resolver.html#feature-resolver-version-2)
/// support. This generator can be removed if the following is resolved:
/// <https://github.com/rust-lang/cargo/issues/9863>
pub(crate) struct TreeResolver {
    /// The path to a `cargo` binary
    cargo_bin: Cargo,
}

impl TreeResolver {
    pub(crate) fn new(cargo_bin: Cargo) -> Self {
        Self { cargo_bin }
    }

    /// Execute `cargo tree` for each target triple and return the stdout
    /// streams containing structured output.
    fn execute_cargo_tree(
        &self,
        manifest_path: &Path,
        host_triples: &BTreeSet<TargetTriple>,
        target_triples: &BTreeSet<TargetTriple>,
        rustc_wrapper: &Path,
    ) -> Result<BTreeMap<TargetTriple, BTreeMap<TargetTriple, Vec<u8>>>> {
        // A collection of all stdout logs from each process
        let mut stdouts: BTreeMap<TargetTriple, BTreeMap<TargetTriple, Vec<u8>>> = BTreeMap::new();

        // We only want to spawn processes for unique cargo platforms
        let mut cargo_host_triples = BTreeMap::<String, BTreeSet<&TargetTriple>>::new();
        for triple in host_triples {
            cargo_host_triples
                .entry(triple.to_cargo())
                .or_default()
                .insert(triple);
        }
        let mut cargo_target_triples = BTreeMap::<String, BTreeSet<&TargetTriple>>::new();
        for triple in target_triples {
            cargo_target_triples
                .entry(triple.to_cargo())
                .or_default()
                .insert(triple);
        }

        for host_triple in cargo_host_triples.keys() {
            // Note that for each host triple `cargo tree` child processes are spawned and then
            // immediately waited upon so that we don't end up with `{HOST_TRIPLES} * {TARGET_TRIPLES}`
            // number of processes (which can be +400 and hit operating system limitations).
            let mut target_triple_to_child = BTreeMap::<String, Child>::new();

            for target_triple in cargo_target_triples.keys() {
                // We use `cargo tree` here because `cargo metadata` doesn't report
                // back target-specific features (enabled with `resolver = "2"`).
                // This is unfortunately a bit of a hack. See:
                // - https://github.com/rust-lang/cargo/issues/9863
                // - https://github.com/bazelbuild/rules_rust/issues/1662
                let child = self
                    .cargo_bin
                    .command()?
                    // These next two environment variables are used to hack cargo into using a custom
                    // host triple instead of the host triple detected by rustc.
                    .env("RUSTC_WRAPPER", rustc_wrapper)
                    .env("HOST_TRIPLE", host_triple)
                    .env("CARGO_CACHE_RUSTC_INFO", "0")
                    .current_dir(manifest_path.parent().expect("All manifests should have a valid parent."))
                    .arg("tree")
                    .arg("--manifest-path")
                    .arg(manifest_path)
                    .arg("--edges")
                    .arg("normal,build,dev")
                    .arg("--prefix=indent")
                    // https://doc.rust-lang.org/cargo/commands/cargo-tree.html#tree-formatting-options
                    .arg("--format=;{p};{f};")
                    .arg("--color=never")
                    .arg("--charset=ascii")
                    .arg("--workspace")
                    .arg("--target")
                    .arg(target_triple)
                    .stdout(std::process::Stdio::piped())
                    .stderr(std::process::Stdio::piped())
                    .spawn()
                    .with_context(|| {
                        format!(
                            "Error spawning cargo in child process to compute features for target '{}', manifest path '{}'",
                            target_triple,
                            manifest_path.display()
                        )
                    })?;
                target_triple_to_child.insert(target_triple.clone(), child);
            }

            debug!(
                "Spawned `cargo tree` processes for host `{}`: {}",
                host_triple,
                target_triple_to_child.len(),
            );

            for (target_triple, child) in target_triple_to_child.into_iter() {
                let output = child.wait_with_output().with_context(|| {
                    format!(
                        "Error running `cargo tree --target={}` (host = '{}'), manifest path '{}'",
                        target_triple,
                        host_triple,
                        manifest_path.display()
                    )
                })?;
                if !output.status.success() {
                    eprintln!("{}", String::from_utf8_lossy(&output.stdout));
                    eprintln!("{}", String::from_utf8_lossy(&output.stderr));
                    bail!(format!("Failed to run cargo tree: {}", output.status))
                }

                // Replicate outputs for any de-duplicated platforms
                for host_plat in cargo_host_triples[host_triple].iter() {
                    for target_plat in cargo_target_triples[&target_triple].iter() {
                        stdouts
                            .entry((*host_plat).clone())
                            .or_default()
                            .insert((*target_plat).clone(), output.stdout.clone());
                    }
                }
            }
        }

        Ok(stdouts)
    }

    // The use of this wrapper should __never__ escape this class.
    #[cfg(target_family = "windows")]
    fn create_rustc_wrapper_impl(output_dir: &Path) -> Result<PathBuf> {
        let wrapper = output_dir.join("cargo_tree_rustc_wrapper.bat");
        std::fs::write(
            &wrapper,
            include_str!(concat!(
                env!("CARGO_MANIFEST_DIR",),
                "/src/metadata/cargo_tree_rustc_wrapper.bat"
            )),
        )
        .context("Failed to write rustc wrapper")?;

        Ok(wrapper)
    }

    // The use of this wrapper should __never__ escape this class.
    #[cfg(target_family = "unix")]
    fn create_rustc_wrapper_impl(output_dir: &Path) -> Result<PathBuf> {
        let wrapper = output_dir.join("cargo_tree_rustc_wrapper.sh");
        std::fs::write(
            &wrapper,
            include_str!(concat!(
                env!("CARGO_MANIFEST_DIR",),
                "/src/metadata/cargo_tree_rustc_wrapper.sh"
            )),
        )
        .context("Failed to write rustc wrapper")?;

        use std::os::unix::fs::PermissionsExt;

        let perms = std::fs::Permissions::from_mode(0o700);
        std::fs::set_permissions(&wrapper, perms)
            .context("Failed to modify permissions of rustc wrapper")?;

        Ok(wrapper)
    }

    /// Create a wrapper for `rustc` which can intercept commands used to identify the host
    /// platform and instead provide details for a specified platform triple.
    ///
    /// Currently Cargo does not have a subcommand that can provide enough information about
    /// builds for all combinations of exec/host and target platforms. Unfortunately, `cargo tree`
    /// (or any other subcommand) does not provide a way to to request metadata from the perspective
    /// of another host platform. An example being, attempting to query the build graph on a Linux
    /// machine for a build that would run on `aarch64-apple-darwin` and target `wasm32-unknown-unknown`.
    /// When a build script or proc-macro is encountered in the build graph it is evaluated for
    /// the current platform running `cargo tree`.
    ///
    /// The script created here takes advantage of the use of [`RUSTC_WRAPPER`](https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-reads)
    /// in Cargo to intercept some known commands Cargo uses to identify the host platform. Namely the
    /// following two:
    ///
    /// [@cargo//src/cargo/util/rustc.rs#L61-L65](https://github.com/rust-lang/cargo/blob/0.81.0/src/cargo/util/rustc.rs#L61-L65)
    /// ```ignore
    /// rust -vV
    /// ```
    ///
    /// Used to identify information about the rustc binary. This output includes a `host: ${PLATFORM_TRIPLE}`
    /// string which identifies the host for the rest of the `cargo tree` command. The created script will
    /// intercept this command and replace the `host:` string with `host: ${HOST_TRIPLE}` where `HOST_TRIPLE`
    /// is an environment variable set by this class.
    ///
    /// [@cargo//src/cargo/core/compiler/build_context/target_info.rs#L168-L218](https://github.com/rust-lang/cargo/blob/0.81.0/src/cargo/core/compiler/build_context/target_info.rs#L168-L218)
    /// ```ignore
    /// rust - --crate-name ___ --print=file-names
    /// ```
    ///
    /// As seen in the associated `target_info.rs` link, this command is generally a lot more complicated but
    /// this prefix is what's used to identify the command to intercept. Cargo will make this call multiple times
    /// to identify a myriad of information about a target platform (primarily [--print=cfg](https://doc.rust-lang.org/rustc/command-line-arguments.html#--print-print-compiler-information)
    /// for the interests here) and will pass `--target ${TARGET_TRIPLE}` in the case where `cargo tree` was
    /// called with the `--target` argument. When identifying host platform information, the `rustc` invocation
    /// above, despite any additional flags, will not contain a `--target` flag. This command is intercepted and
    /// indicates platform info is being requested. When intercepted a `--target ${HOST_TRIPLE}` argument will be
    /// appended where `HOST_TRIPLE` is an environment variable set by this class.
    ///
    /// The design/use of this script feels blasphemous but it is the only way I could figure out how to get the
    /// necessary information from Cargo without reimplementing the dependency+feature resolver logic. This is
    /// valuable in allowing `cargo-bazel` to scale with different versions of Rust.
    ///
    /// This wrapper can probably be eliminated if the following feature request is implemented:
    /// - <https://github.com/rust-lang/cargo/issues/14527>
    fn create_rustc_wrapper(output_dir: &Path) -> Result<PathBuf> {
        Self::create_rustc_wrapper_impl(output_dir)
    }

    /// Computes the set of enabled features for each target triplet for each crate.
    #[tracing::instrument(name = "TreeResolver::generate", skip_all)]
    pub(crate) fn generate(
        &self,
        pristine_manifest_path: &Utf8Path,
        target_triples: &BTreeSet<TargetTriple>,
    ) -> Result<TreeResolverMetadata> {
        debug!(
            "Generating features for manifest {}",
            pristine_manifest_path
        );

        let tempdir = tempfile::tempdir().context("Failed to make tempdir")?;

        let manifest_path_with_transitive_proc_macros = self
            .copy_project_with_explicit_deps_on_all_transitive_proc_macros(
                pristine_manifest_path,
                &tempdir.path().join("explicit_proc_macro_deps"),
            )
            .context("Failed to copy project with proc macro deps made direct")?;

        let rustc_wrapper = Self::create_rustc_wrapper(tempdir.path())?;

        let host_triples: BTreeSet<TargetTriple> = target_triples
            .iter()
            // Only query triples for platforms that have host tools.
            .filter(|host_triple| {
                RUSTC_TRIPLES_WITH_HOST_TOOLS.contains(&host_triple.to_cargo().as_str())
            })
            .cloned()
            .collect();

        // This is a very expensive process. Here we iterate over all target triples
        // and generate tree data as though they were also the host triple
        let deps_tree_streams: BTreeMap<TargetTriple, BTreeMap<TargetTriple, Vec<u8>>> = self
            .execute_cargo_tree(
                &manifest_path_with_transitive_proc_macros,
                &host_triples,
                target_triples,
                &rustc_wrapper,
            )?;

        let mut metadata: BTreeMap<CrateId, BTreeMap<TargetTriple, CargoTreeEntry>> =
            BTreeMap::new();

        for (host_triple, target_streams) in deps_tree_streams.into_iter() {
            for (target_triple, stdout) in target_streams.into_iter() {
                debug!(
                    "Parsing (host={}) `cargo tree --target {}` output:\n```\n{}\n```",
                    host_triple,
                    target_triple,
                    String::from_utf8_lossy(&stdout),
                );

                let (target_tree_data, host_tree_data) = parse_cargo_tree_output(stdout.lines())?;

                for (entry, tree_data) in target_tree_data {
                    metadata
                        .entry(entry.as_crate_id().clone())
                        .or_default()
                        .entry(target_triple.clone())
                        .or_default()
                        .consume(tree_data);
                }
                for (entry, tree_data) in host_tree_data {
                    metadata
                        .entry(entry.as_crate_id().clone())
                        .or_default()
                        .entry(host_triple.clone())
                        .or_default()
                        .consume(tree_data);
                }
            }
        }

        // Collect all metadata into a mapping of crate to it's metadata per target.
        let mut result = TreeResolverMetadata::new();
        for (crate_id, tree_data) in metadata.into_iter() {
            let common = CargoTreeEntry {
                features: tree_data
                    .iter()
                    .fold(
                        None,
                        |common: Option<BTreeSet<String>>, (_, data)| match common {
                            Some(common) => {
                                Some(common.intersection(&data.features).cloned().collect())
                            }
                            None => Some(data.features.clone()),
                        },
                    )
                    .unwrap_or_default(),
                deps: tree_data
                    .iter()
                    .fold(
                        None,
                        |common: Option<BTreeSet<CrateId>>, (_, data)| match common {
                            Some(common) => {
                                Some(common.intersection(&data.deps).cloned().collect())
                            }
                            None => Some(data.deps.clone()),
                        },
                    )
                    .unwrap_or_default(),
            };
            let mut select: Select<CargoTreeEntry> = Select::default();
            for (target_triple, data) in tree_data {
                let mut entry = CargoTreeEntry::new();
                entry.features.extend(
                    data.features
                        .into_iter()
                        .filter(|f| !common.features.contains(f)),
                );
                entry
                    .deps
                    .extend(data.deps.into_iter().filter(|d| !common.deps.contains(d)));
                if !entry.is_empty() {
                    select.insert(entry, Some(target_triple.to_bazel()));
                }
            }
            if !common.is_empty() {
                select.insert(common, None);
            }
            result.insert(crate_id, select);
        }
        Ok(result)
    }

    // Artificially inject all proc macros as dependency roots.
    // Proc macros are built in the exec rather than target configuration.
    // If we do cross-compilation, these will be different, and it will be important that we have resolved features and optional dependencies for the exec platform.
    // If we don't treat proc macros as roots for the purposes of resolving, we may end up with incorrect platform-specific features.
    //
    // Example:
    // If crate foo only uses a proc macro Linux,
    // and that proc-macro depends on syn and requires the feature extra-traits,
    // when we resolve on macOS we'll see we don't need the extra-traits feature of syn because the proc macro isn't used.
    // But if we're cross-compiling for Linux from macOS, we'll build a syn, but because we're building it for macOS (because proc macros are exec-cfg dependencies),
    // we'll build syn but _without_ the extra-traits feature (because our resolve told us it was Linux only).
    //
    // By artificially injecting all proc macros as root dependencies,
    // it means we are forced to resolve the dependencies and features for those proc-macros on all platforms we care about,
    // even if they wouldn't be used in some platform when cfg == exec.
    //
    // This is tested by the "keyring" example in examples/musl_cross_compiling - the keyring crate uses proc-macros only on Linux,
    // and if we don't have this fake root injection, cross-compiling from Darwin to Linux won't work because features don't get correctly resolved for the exec=darwin case.
    fn copy_project_with_explicit_deps_on_all_transitive_proc_macros(
        &self,
        pristine_manifest_path: &Utf8Path,
        output_dir: &Path,
    ) -> Result<PathBuf> {
        if !output_dir.exists() {
            std::fs::create_dir_all(output_dir)?;
        }

        let pristine_root = pristine_manifest_path.parent().unwrap();
        for file in std::fs::read_dir(pristine_root).context("Failed to read dir")? {
            let source_path = file?.path();
            let file_name = source_path.file_name().unwrap();
            if file_name != "Cargo.toml" && file_name != "Cargo.lock" {
                let destination = output_dir.join(file_name);
                symlink(&source_path, &destination).with_context(|| {
                    format!(
                        "Failed to create symlink {:?} pointing at {:?}",
                        destination, source_path
                    )
                })?;
            }
        }
        std::fs::copy(
            pristine_root.join("Cargo.lock"),
            output_dir.join("Cargo.lock"),
        )
        .with_context(|| {
            format!(
                "Failed to copy Cargo.lock from {:?} to {:?}",
                pristine_root, output_dir
            )
        })?;

        let cargo_metadata = self
            .cargo_bin
            .metadata_command_with_options(
                pristine_manifest_path.as_std_path(),
                vec!["--locked".to_owned()],
            )?
            .manifest_path(pristine_manifest_path.as_std_path())
            .exec()
            .context("Failed to run cargo metadata to list transitive proc macros")?;
        let proc_macros = cargo_metadata
            .packages
            .iter()
            .filter(|p| {
                p.targets.iter().any(|t| {
                    t.kind
                        .iter()
                        .any(|k| matches!(k, cargo_metadata::TargetKind::ProcMacro))
                })
            })
            // Filter out any in-workspace proc macros, populate dependency details for non-in-workspace proc macros.
            .filter_map(|pm| {
                if let Some(source) = pm.source.as_ref() {
                    let mut detail = DependencyDetailWithOrd(cargo_toml::DependencyDetail {
                        package: Some(pm.name.clone()),
                        // Don't forcibly enable default features - if some other dependency enables them, they will still be enabled.
                        default_features: false,
                        ..cargo_toml::DependencyDetail::default()
                    });

                    let source = match Source::parse(&source.repr, pm.version.to_string()) {
                        Ok(source) => source,
                        Err(err) => {
                            return Some(Err(err));
                        }
                    };
                    source.populate_details(&mut detail.0);

                    Some(Ok((pm.name.clone(), detail)))
                } else {
                    None
                }
            })
            .collect::<Result<BTreeSet<_>>>()?;

        let mut manifest = cargo_toml::Manifest::from_path(pristine_manifest_path.as_std_path())
            .with_context(|| {
                format!(
                    "Failed to parse Cargo.toml file at {}",
                    pristine_manifest_path
                )
            })?;

        // To add dependencies to a virtual workspace, we need to add them to a package inside the workspace,
        // we can't just add them to the workspace directly.
        if !proc_macros.is_empty() && manifest.package.is_none() {
            if let Some(ref mut workspace) = &mut manifest.workspace {
                if !workspace.members.contains(&".".to_owned()) {
                    workspace.members.push(".".to_owned());
                }
                manifest.package = Some(cargo_toml::Package::new(
                    "rules_rust_fake_proc_macro_root",
                    "0.0.0",
                ));
            }
            if manifest.lib.is_none() && manifest.bin.is_empty() {
                manifest.bin.push(cargo_toml::Product {
                    name: Some("rules_rust_fake_proc_macro_root_bin".to_owned()),
                    path: Some("/dev/null".to_owned()),
                    ..cargo_toml::Product::default()
                })
            }
        }

        let mut count_map: HashMap<_, u64> = HashMap::new();
        for (dep_name, detail) in proc_macros {
            let count = count_map.entry(dep_name.clone()).or_default();
            manifest.dependencies.insert(
                format!("rules_rust_fake_proc_macro_root_{}_{}", dep_name, count),
                cargo_toml::Dependency::Detailed(Box::new(detail.0)),
            );
            *count += 1;
        }
        let manifest_path_with_transitive_proc_macros = output_dir.join("Cargo.toml");
        crate::splicing::write_manifest(&manifest_path_with_transitive_proc_macros, &manifest)?;
        Ok(manifest_path_with_transitive_proc_macros)
    }
}

#[derive(Debug, PartialEq, Eq)]
enum Source {
    Registry {
        registry: String,
        version: String,
    },
    Git {
        git: String,
        rev: Option<String>,
        branch: Option<String>,
        tag: Option<String>,
    },
}

impl Source {
    fn parse(string: &str, version: String) -> Result<Source> {
        let url: Url = Url::parse(string)?;
        let original_scheme = url.scheme().to_owned();
        let scheme_parts: Vec<_> = original_scheme.split('+').collect();
        match &scheme_parts[..] {
            // e.g. sparse+https://github.com/rust-lang/crates.io-index
            ["sparse", _] => Ok(Self::Registry {
                registry: url.to_string(),
                version,
            }),
            // e.g. registry+https://github.com/rust-lang/crates.io-index
            ["registry", scheme] => {
                let new_url = set_url_scheme_despite_the_url_crate_not_wanting_us_to(&url, scheme)?;
                Ok(Self::Registry {
                    registry: new_url,
                    version,
                })
            }
            // e.g. git+https://github.com/serde-rs/serde.git?rev=9b868ef831c95f50dd4bde51a7eb52e3b9ee265a#9b868ef831c95f50dd4bde51a7eb52e3b9ee265a
            ["git", scheme] => {
                let mut query: HashMap<String, String> = url
                    .query_pairs()
                    .map(|(k, v)| (k.into_owned(), v.into_owned()))
                    .collect();

                let mut url = url;
                url.set_fragment(None);
                url.set_query(None);
                let new_url = set_url_scheme_despite_the_url_crate_not_wanting_us_to(&url, scheme)?;

                Ok(Self::Git {
                    git: new_url,
                    rev: query.remove("rev"),
                    branch: query.remove("branch"),
                    tag: query.remove("tag"),
                })
            }
            _ => {
                anyhow::bail!(
                    "Couldn't parse source {:?}: Didn't recognise scheme",
                    string
                );
            }
        }
    }

    fn populate_details(self, details: &mut cargo_toml::DependencyDetail) {
        match self {
            Self::Registry { registry, version } => {
                details.registry_index = Some(registry);
                details.version = Some(version);
            }
            Self::Git {
                git,
                rev,
                branch,
                tag,
            } => {
                details.git = Some(git);
                details.rev = rev;
                details.branch = branch;
                details.tag = tag;
            }
        }
    }
}

fn set_url_scheme_despite_the_url_crate_not_wanting_us_to(
    url: &Url,
    new_scheme: &str,
) -> Result<String> {
    let (_old_scheme, new_url_without_scheme) = url.as_str().split_once(':').ok_or_else(|| {
        anyhow::anyhow!(
            "Cannot set scheme of URL which doesn't contain \":\": {:?}",
            url
        )
    })?;
    Ok(format!("{new_scheme}:{new_url_without_scheme}"))
}

// cargo_toml::DependencyDetail doesn't implement PartialOrd/Ord so can't be put in a sorted collection.
// Wrap it so we can sort things for stable orderings.
#[derive(Debug, PartialEq)]
struct DependencyDetailWithOrd(cargo_toml::DependencyDetail);

impl PartialOrd for DependencyDetailWithOrd {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for DependencyDetailWithOrd {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        let cargo_toml::DependencyDetail {
            version,
            registry,
            registry_index,
            path,
            inherited,
            git,
            branch,
            tag,
            rev,
            features,
            optional,
            default_features,
            package,
            unstable: _,
        } = &self.0;

        version
            .cmp(&other.0.version)
            .then(registry.cmp(&other.0.registry))
            .then(registry_index.cmp(&other.0.registry_index))
            .then(path.cmp(&other.0.path))
            .then(inherited.cmp(&other.0.inherited))
            .then(git.cmp(&other.0.git))
            .then(branch.cmp(&other.0.branch))
            .then(tag.cmp(&other.0.tag))
            .then(rev.cmp(&other.0.rev))
            .then(features.cmp(&other.0.features))
            .then(optional.cmp(&other.0.optional))
            .then(default_features.cmp(&other.0.default_features))
            .then(package.cmp(&other.0.package))
    }
}

impl Eq for DependencyDetailWithOrd {}

/// A wrapper for [CrateId] used by [parse_cargo_tree_output] to successfully
/// parse target and host dependencies.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum TreeDepCompileKind {
    /// Collecting dependencies for the target platform.
    Target(CrateId),

    /// Collecting dependencies for the host platform (e.g. `[build-dependency]`
    /// and `proc-macro`).
    Host(CrateId),

    /// A variant of [TreeDepCompileKind::Target] that represents an edge where
    /// dependencies are to be collected for host. E.g. a crate which has
    /// a `[build-dependency] or `(proc-macro)` dependency. This variant is only
    /// used for internal bookkeeping to make sure other nodes farther down in
    /// the graph are collected as [TreeDepCompileKind::Host].
    TargetWithHostDep(CrateId),
}

impl TreeDepCompileKind {
    pub fn new(crate_id: CrateId, is_host_dep: bool) -> Self {
        if is_host_dep {
            TreeDepCompileKind::Host(crate_id)
        } else {
            TreeDepCompileKind::Target(crate_id)
        }
    }

    pub fn as_crate_id(&self) -> &CrateId {
        match self {
            TreeDepCompileKind::Target(id) => id,
            TreeDepCompileKind::TargetWithHostDep(id) => id,
            TreeDepCompileKind::Host(id) => id,
        }
    }
}

impl From<TreeDepCompileKind> for CrateId {
    fn from(value: TreeDepCompileKind) -> Self {
        match value {
            TreeDepCompileKind::Target(id) => id,
            TreeDepCompileKind::TargetWithHostDep(id) => id,
            TreeDepCompileKind::Host(id) => id,
        }
    }
}

/// Parses the output of `cargo tree --format=|{p}|{f}|`. Other flags may be
/// passed to `cargo tree` as well, but this format is critical.
fn parse_cargo_tree_output<I, S, E>(
    lines: I,
) -> Result<(
    BTreeMap<TreeDepCompileKind, CargoTreeEntry>,
    BTreeMap<TreeDepCompileKind, CargoTreeEntry>,
)>
where
    I: Iterator<Item = std::result::Result<S, E>>,
    S: AsRef<str>,
    E: std::error::Error + Sync + Send + 'static,
{
    let mut target_tree_data = BTreeMap::<TreeDepCompileKind, CargoTreeEntry>::new();
    let mut host_tree_data: BTreeMap<TreeDepCompileKind, CargoTreeEntry> = BTreeMap::new();
    let mut parents: Vec<TreeDepCompileKind> = Vec::new();

    let is_host_child = |parents: &Vec<TreeDepCompileKind>| {
        parents.iter().any(|p| match p {
            TreeDepCompileKind::Target(_) => false,
            TreeDepCompileKind::TargetWithHostDep(_) => true,
            TreeDepCompileKind::Host(_) => true,
        })
    };

    for line in lines {
        let line = line?;
        let line = line.as_ref();
        if line.is_empty() {
            continue;
        }

        let parts = line.split(';').collect::<Vec<_>>();
        if parts.len() != 4 {
            // The only time a line will not cleanly contain 4 parts
            // is when there's a build dependencies divider. When found,
            // start tracking build dependencies.
            if line.ends_with("[build-dependencies]") {
                let build_depth =
                    (line.chars().count() - "[build-dependencies]".chars().count()) / 4;

                if matches!(parents[build_depth], TreeDepCompileKind::Target(_)) {
                    parents[build_depth] =
                        TreeDepCompileKind::TargetWithHostDep(parents[build_depth].clone().into());
                }

                continue;
            } else if line.ends_with("[dev-dependencies]") {
                // Dev dependencies are not treated any differently than normal dependencies
                // when we enter these blocks, continue to collect deps as usual.
                continue;
            }
            bail!("Unexpected line '{}'", line);
        }
        // We expect the crate id (parts[1]) to be one of:
        // `<crate name> v<crate version>`
        // `<crate name> v<crate version> (<path>)`
        // `<crate name> v<crate version> (proc-macro)`
        // `<crate name> v<crate version> (proc-macro) (<path>)`
        // https://github.com/rust-lang/cargo/blob/19f952f160d4f750d1e12fad2bf45e995719673d/src/cargo/ops/tree/mod.rs#L281
        let crate_id_parts = parts[1].split(' ').collect::<Vec<_>>();
        if crate_id_parts.len() < 2 && crate_id_parts.len() > 4 {
            bail!(
                "Unexpected crate id format '{}' when parsing 'cargo tree' output.",
                parts[1]
            );
        }
        let version_str = crate_id_parts[1].strip_prefix('v').ok_or_else(|| {
            anyhow!(
                "Unexpected crate version '{}' when parsing 'cargo tree' output.",
                crate_id_parts[1]
            )
        })?;
        let version = Version::parse(version_str).context("Failed to parse version")?;
        let crate_id = CrateId::new(crate_id_parts[0].to_owned(), version);
        let is_proc_macro = crate_id_parts.len() > 2 && crate_id_parts[2] == "(proc-macro)";

        // Update bookkeeping for dependency tracking. Note that the `cargo tree --prefix=indent`
        // output is expected to have 4 characters per section. We only care about depth but cannot
        // use `--prefix=depth` because it does not show the `[build-dependencies]` section which we
        // need to identify when build dependencies start.
        let depth = parts[0].chars().count() / 4;

        let (kind, is_host_dep) = if (depth + 1) <= parents.len() {
            // Drain parents until we get down to the right depth
            let range = parents.len() - (depth + 1);
            for _ in 0..range {
                parents.pop();
            }

            // If the current parent does not have the same Crate ID, then
            // it's likely we have moved to a different crate. This can happen
            // in the following case
            // ```
            // ├── proc-macro2 v1.0.81
            // │   └── unicode-ident v1.0.12
            // ├── quote v1.0.36
            // │   └── proc-macro2 v1.0.81 (*)
            // ```
            if parents
                .last()
                .filter(|last| *last.as_crate_id() != crate_id)
                .is_some()
            {
                parents.pop();

                // Because we pop a parent we need to check at this time if the current crate is
                // truly a host dependency.
                let is_host_dep = is_proc_macro || is_host_child(&parents);
                let kind = TreeDepCompileKind::new(crate_id, is_host_dep);

                parents.push(kind.clone());

                (kind, is_host_dep)
            } else {
                let is_host_dep = is_proc_macro || is_host_child(&parents);
                let kind = TreeDepCompileKind::new(crate_id, is_host_dep);
                (kind, is_host_dep)
            }
        } else {
            let is_host_dep = is_proc_macro || is_host_child(&parents);
            let kind = if is_host_dep {
                TreeDepCompileKind::Host(crate_id)
            } else {
                TreeDepCompileKind::Target(crate_id)
            };

            // Start tracking the current crate as the new parent for any
            // crates that represent a new depth in the dep tree.
            parents.push(kind.clone());

            (kind, is_host_dep)
        };

        let mut features = if parts[2].is_empty() {
            BTreeSet::new()
        } else {
            parts[2].split(',').map(str::to_owned).collect()
        };

        // Attribute any dependency that is not the root to it's parent.
        if depth > 0 {
            // Access the last item in the list of parents and insert the current
            // crate as a dependency to it.
            if let Some(parent) = parents.iter().rev().nth(1) {
                // Ensure this variant is never referred to publicly
                let sanitized_compile_kind = |parent: &TreeDepCompileKind| match parent {
                    TreeDepCompileKind::Target(_) => parent.clone(),
                    TreeDepCompileKind::TargetWithHostDep(p) => {
                        TreeDepCompileKind::Target(p.clone())
                    }
                    TreeDepCompileKind::Host(_) => parent.clone(),
                };

                // Dependency data is only tracked for direct consumers of build dependencies
                // since they're known to be wrong cross-platform.
                match parent {
                    TreeDepCompileKind::Target(_) => &mut target_tree_data,
                    TreeDepCompileKind::TargetWithHostDep(_) => &mut target_tree_data,
                    TreeDepCompileKind::Host(_) => &mut host_tree_data,
                }
                .entry(sanitized_compile_kind(parent))
                .or_default()
                .deps
                .insert(kind.as_crate_id().clone());
            }
        }

        if is_host_dep {
            &mut host_tree_data
        } else {
            &mut target_tree_data
        }
        .entry(kind)
        .or_default()
        .features
        .append(&mut features);
    }
    Ok((target_tree_data, host_tree_data))
}

#[cfg(test)]
mod test {
    use std::process;

    use textwrap::dedent;

    use super::*;

    #[test]
    fn parse_sparse_source() {
        let source = Source::parse(
            "sparse+https://github.com/rust-lang/crates.io-index",
            "1.0.1".to_owned(),
        )
        .unwrap();
        // sparse we want to leave the augmented scheme in there.
        // cargo_toml::DependencyDetail::registry_index explicitly supports
        // sparse+https:
        assert_eq!(
            source,
            Source::Registry {
                registry: "sparse+https://github.com/rust-lang/crates.io-index".to_owned(),
                version: "1.0.1".to_owned()
            }
        );
    }

    #[test]
    fn parse_registry_source() {
        let source = Source::parse(
            "registry+https://github.com/rust-lang/crates.io-index",
            "1.0.1".to_owned(),
        )
        .unwrap();
        assert_eq!(
            source,
            Source::Registry {
                registry: "https://github.com/rust-lang/crates.io-index".to_owned(),
                version: "1.0.1".to_owned()
            }
        );
    }

    #[test]
    fn parse_git_source() {
        let source = Source::parse("git+https://github.com/serde-rs/serde.git?rev=9b868ef831c95f50dd4bde51a7eb52e3b9ee265a#9b868ef831c95f50dd4bde51a7eb52e3b9ee265a", "unused".to_owned()).unwrap();
        assert_eq!(
            source,
            Source::Git {
                git: "https://github.com/serde-rs/serde.git".to_owned(),
                rev: Some("9b868ef831c95f50dd4bde51a7eb52e3b9ee265a".to_owned()),
                branch: None,
                tag: None,
            }
        );
    }

    #[cfg(unix)]
    fn create_mock_rustc(output_dir: &Path, text: &str) -> (PathBuf, PathBuf) {
        let wrapper = output_dir.join("rustc");

        std::fs::write(&wrapper, text).unwrap();

        use std::os::unix::fs::PermissionsExt;

        let perms = std::fs::Permissions::from_mode(0o700);
        std::fs::set_permissions(&wrapper, perms).unwrap();

        let rustc_wrapper = TreeResolver::create_rustc_wrapper(output_dir).unwrap();

        (wrapper, rustc_wrapper)
    }

    #[cfg(windows)]
    fn create_mock_rustc(output_dir: &Path, text: &str) -> (PathBuf, PathBuf) {
        let wrapper = output_dir.join("rustc.bat");

        std::fs::write(&wrapper, text).unwrap();

        let rustc_wrapper = TreeResolver::create_rustc_wrapper(output_dir).unwrap();

        // For testing, the rustc executable is a batch script and not a compiled executable.
        // any strings referring to it as an executable will need to be updated.
        let content = std::fs::read_to_string(&rustc_wrapper).unwrap();
        std::fs::write(&rustc_wrapper, content.replace(".exe", ".bat")).unwrap();

        (wrapper, rustc_wrapper)
    }

    fn new_mock_rustc_command(
        rustc_wrapper: &Path,
        rustc: &Path,
        workdir: &Path,
    ) -> process::Command {
        let mut command = process::Command::new(rustc_wrapper);
        command
            .current_dir(workdir)
            .env("RUSTC_WRAPPER", rustc_wrapper)
            .env("RUSTC", rustc)
            .env("HOST_TRIPLE", "mock-platform-triple")
            .arg(rustc);
        command
    }

    #[test]
    fn cargo_tree_rustc_wrapper_vv() {
        let (_, tempdir) = crate::test::test_tempdir("cargo_tree_rustc_wrapper_vv");

        let text = dedent(
            r#"
            rustc 1.81.0 (eeb90cda1 2024-09-04)
            binary: rustc
            commit-hash: eeb90cda1969383f56a2637cbd3037bdf598841c
            commit-date: 2024-09-04
            host: x86_64-unknown-linux-gnu
            release: 1.81.0
            LLVM version: 18.1.7
            "#,
        );

        let script_text = if cfg!(windows) {
            dedent(
                r#"
                @ECHO OFF
                "#,
            ) + &text
                .lines()
                .map(|l| "echo ".to_owned() + l)
                .collect::<Vec<_>>()
                .join("\n")
        } else {
            dedent(
                r#"
                #!/usr/bin/env bash
                set -euo pipefail
                cat << EOF
                "#,
            ) + &text
                + "\nEOF\n"
        };

        let (rustc, rustc_wrapper) = create_mock_rustc(&tempdir, &script_text);

        let output = new_mock_rustc_command(&rustc_wrapper, &rustc, &tempdir)
            .arg("-vV")
            .output()
            .unwrap();

        assert_eq!(
            output.status.code().unwrap(),
            0,
            "Exit with {}\n{}\n{}",
            output.status.code().unwrap(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr),
        );

        let stdout = String::from_utf8(output.stdout).unwrap();

        assert!(
            stdout.contains("mock-platform-triple") && !stdout.contains("x86_64-unknown-linux-gnu"),
            "The rustc wrapper did not replace the host platform\n{}",
            stdout
        )
    }

    #[test]
    fn cargo_tree_rustc_wrapper_target_inject() {
        let (_, tempdir) = crate::test::test_tempdir("cargo_tree_rustc_wrapper_target_inject");

        let script_text = if cfg!(windows) {
            dedent(
                r#"
                @ECHO OFF
                echo %*
                "#,
            )
        } else {
            dedent(
                r#"
                #!/usr/bin/env bash
                set -euo pipefail
                echo "$@"
                "#,
            )
        };

        let (rustc, rustc_wrapper) = create_mock_rustc(&tempdir, &script_text);

        let output = new_mock_rustc_command(&rustc_wrapper, &rustc, &tempdir)
            .arg("-")
            .arg("--crate-name")
            .arg("___")
            .arg("--print=file-names")
            .arg("--print=cfg")
            .output()
            .unwrap();

        assert_eq!(
            output.status.code().unwrap(),
            0,
            "Exit with {}\n{}\n{}",
            output.status.code().unwrap(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr),
        );

        let stdout = String::from_utf8(output.stdout).unwrap();

        assert!(
            stdout.contains("--target mock-platform-triple"),
            "The rustc wrapper did not inject `--target`. Got output:\n{}",
            stdout
        )
    }

    #[test]
    fn cargo_tree_rustc_wrapper_fallthrough() {
        let (_, tempdir) = crate::test::test_tempdir("cargo_tree_rustc_wrapper_fallthrough");

        let script_text = if cfg!(windows) {
            dedent(
                r#"
                @ECHO OFF
                echo %*
                "#,
            )
        } else {
            dedent(
                r#"
                #!/usr/bin/env bash
                set -euo pipefail
                echo "$@"
                "#,
            )
        };

        let (rustc, rustc_wrapper) = create_mock_rustc(&tempdir, &script_text);

        for args in [
            vec!["-vV", "--help"],
            vec![
                "-",
                "--crate-name",
                "___",
                "--print=file-names",
                "--target",
                "wasm64-unknown-unknown",
            ],
        ] {
            let output = new_mock_rustc_command(&rustc_wrapper, &rustc, &tempdir)
                .args(args.clone())
                .output()
                .unwrap();

            assert_eq!(
                output.status.code().unwrap(),
                0,
                "Exit with {}\n{}\n{}",
                output.status.code().unwrap(),
                String::from_utf8_lossy(&output.stdout),
                String::from_utf8_lossy(&output.stderr),
            );

            // For some reason Windows is putting quotes around some strings. Given
            // the set of inputs, this can be safely stripped
            let stdout = String::from_utf8(output.stdout).unwrap().replace('"', "");

            let expected = args.join(" ");

            assert!(
                stdout.trim().ends_with(&expected),
                "The rustc wrapper output didn't match expectations\nexpected: `{}`\ngot: `{}`",
                expected,
                stdout
            )
        }
    }

    #[test]
    fn serde_cargo_tree_entry() {
        {
            let entry: CargoTreeEntry = serde_json::from_str("{}").unwrap();
            assert_eq!(CargoTreeEntry::new(), entry);
        }
        {
            let entry: CargoTreeEntry =
                serde_json::from_str(r#"{"features": ["default"]}"#).unwrap();
            assert_eq!(
                CargoTreeEntry {
                    features: BTreeSet::from(["default".to_owned()]),
                    deps: BTreeSet::new(),
                },
                entry
            );
        }
        {
            let entry: CargoTreeEntry =
                serde_json::from_str(r#"{"deps": ["common 1.2.3"]}"#).unwrap();
            assert_eq!(
                CargoTreeEntry {
                    features: BTreeSet::new(),
                    deps: BTreeSet::from([CrateId::new(
                        "common".to_owned(),
                        Version::new(1, 2, 3)
                    )]),
                },
                entry
            );
        }
        {
            let entry: CargoTreeEntry =
                serde_json::from_str(r#"{"features": ["default"], "deps": ["common 1.2.3"]}"#)
                    .unwrap();
            assert_eq!(
                CargoTreeEntry {
                    features: BTreeSet::from(["default".to_owned()]),
                    deps: BTreeSet::from([CrateId::new(
                        "common".to_owned(),
                        Version::new(1, 2, 3)
                    )]),
                },
                entry
            );
        }
    }

    #[test]
    fn parse_cargo_tree_output_test() {
        let autocfg_id = CrateId {
            name: "autocfg".to_owned(),
            version: Version::new(1, 2, 0),
        };
        let chrono_id = CrateId {
            name: "chrono".to_owned(),
            version: Version::new(0, 4, 24),
        };
        let core_foundation_sys_id = CrateId {
            name: "core-foundation-sys".to_owned(),
            version: Version::new(0, 8, 6),
        };
        let cpufeatures_id = CrateId {
            name: "cpufeatures".to_owned(),
            version: Version::new(0, 2, 7),
        };
        let iana_time_zone_id = CrateId {
            name: "iana-time-zone".to_owned(),
            version: Version::new(0, 1, 60),
        };
        let libc_id = CrateId {
            name: "libc".to_owned(),
            version: Version::new(0, 2, 153),
        };
        let num_integer_id = CrateId {
            name: "num-integer".to_owned(),
            version: Version::new(0, 1, 46),
        };
        let num_traits_id = CrateId {
            name: "num-traits".to_owned(),
            version: Version::new(0, 2, 18),
        };
        let proc_macro2_id = CrateId {
            name: "proc-macro2".to_owned(),
            version: Version::new(1, 0, 81),
        };
        let quote_id = CrateId {
            name: "quote".to_owned(),
            version: Version::new(1, 0, 36),
        };
        let serde_derive_id = CrateId {
            name: "serde_derive".to_owned(),
            version: Version::new(1, 0, 152),
        };
        let syn_id = CrateId {
            name: "syn".to_owned(),
            version: Version::new(1, 0, 109),
        };
        let time_id = CrateId {
            name: "time".to_owned(),
            version: Version::new(0, 1, 45),
        };
        let tree_data_id = CrateId {
            name: "tree-data".to_owned(),
            version: Version::new(0, 1, 0),
        };
        let unicode_ident_id = CrateId {
            name: "unicode-ident".to_owned(),
            version: Version::new(1, 0, 12),
        };

        let (target_output, host_output) = parse_cargo_tree_output(
            textwrap::dedent(
                r#"
                ;tree-data v0.1.0 (/rules_rust/crate_universe/test_data/metadata/tree_data);;
                |-- ;chrono v0.4.24;clock,default,iana-time-zone,js-sys,oldtime,std,time,wasm-bindgen,wasmbind,winapi;
                |   |-- ;iana-time-zone v0.1.60;fallback;
                |   |   `-- ;core-foundation-sys v0.8.6;default,link;
                |   |-- ;num-integer v0.1.46;;
                |   |   `-- ;num-traits v0.2.18;i128;
                |   |       [build-dependencies]
                |   |       `-- ;autocfg v1.2.0;;
                |   |-- ;num-traits v0.2.18;i128; (*)
                |   `-- ;time v0.1.45;;
                |       `-- ;libc v0.2.153;default,std;
                |-- ;cpufeatures v0.2.7;;
                |   `-- ;libc v0.2.153;default,std;
                `-- ;serde_derive v1.0.152 (proc-macro);default;
                    |-- ;proc-macro2 v1.0.81;default,proc-macro;
                    |   `-- ;unicode-ident v1.0.12;;
                    |-- ;quote v1.0.36;default,proc-macro;
                    |   `-- ;proc-macro2 v1.0.81;default,proc-macro; (*)
                    `-- ;syn v1.0.109;clone-impls,default,derive,parsing,printing,proc-macro,quote;
                        |-- ;proc-macro2 v1.0.81;default,proc-macro; (*)
                        |-- ;quote v1.0.36;default,proc-macro; (*)
                        `-- ;unicode-ident v1.0.12;;

                "#,
            ).lines().map(Ok::<&str, std::io::Error>),
        )
        .unwrap();

        assert_eq!(
            BTreeMap::from([
                (
                    TreeDepCompileKind::Host(autocfg_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::new(),
                    },
                ),
                (
                    TreeDepCompileKind::Host(proc_macro2_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "proc-macro".to_owned()]),
                        deps: BTreeSet::from([unicode_ident_id.clone(),]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(quote_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "proc-macro".to_owned()]),
                        deps: BTreeSet::from([proc_macro2_id.clone()]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(serde_derive_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned()]),
                        deps: BTreeSet::from([
                            proc_macro2_id.clone(),
                            quote_id.clone(),
                            syn_id.clone()
                        ]),
                    }
                ),
                (
                    TreeDepCompileKind::Host(syn_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from([
                            "clone-impls".to_owned(),
                            "default".to_owned(),
                            "derive".to_owned(),
                            "parsing".to_owned(),
                            "printing".to_owned(),
                            "proc-macro".to_owned(),
                            "quote".to_owned()
                        ]),
                        deps: BTreeSet::from([
                            proc_macro2_id.clone(),
                            quote_id.clone(),
                            unicode_ident_id.clone()
                        ]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(unicode_ident_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::new(),
                    },
                ),
            ]),
            host_output,
            "Failed checking host dependencies."
        );

        assert_eq!(
            BTreeMap::from([
                (
                    TreeDepCompileKind::Target(chrono_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from([
                            "clock".to_owned(),
                            "default".to_owned(),
                            "iana-time-zone".to_owned(),
                            "js-sys".to_owned(),
                            "oldtime".to_owned(),
                            "std".to_owned(),
                            "time".to_owned(),
                            "wasm-bindgen".to_owned(),
                            "wasmbind".to_owned(),
                            "winapi".to_owned(),
                        ]),
                        deps: BTreeSet::from([
                            iana_time_zone_id.clone(),
                            num_integer_id.clone(),
                            num_traits_id.clone(),
                            time_id.clone(),
                        ]),
                    }
                ),
                (
                    TreeDepCompileKind::Target(core_foundation_sys_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "link".to_owned()]),
                        deps: BTreeSet::new(),
                    }
                ),
                (
                    TreeDepCompileKind::Target(cpufeatures_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::from([libc_id.clone()]),
                    },
                ),
                (
                    TreeDepCompileKind::Target(iana_time_zone_id),
                    CargoTreeEntry {
                        features: BTreeSet::from(["fallback".to_owned()]),
                        deps: BTreeSet::from([core_foundation_sys_id]),
                    }
                ),
                (
                    TreeDepCompileKind::Target(libc_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "std".to_owned()]),
                        deps: BTreeSet::new(),
                    }
                ),
                (
                    TreeDepCompileKind::Target(num_integer_id),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::from([num_traits_id.clone()]),
                    },
                ),
                (
                    TreeDepCompileKind::Target(num_traits_id),
                    CargoTreeEntry {
                        features: BTreeSet::from(["i128".to_owned()]),
                        deps: BTreeSet::from([autocfg_id.clone()]),
                    }
                ),
                (
                    TreeDepCompileKind::Target(time_id),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::from([libc_id]),
                    }
                ),
                (
                    TreeDepCompileKind::Target(tree_data_id),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::from([chrono_id, cpufeatures_id, serde_derive_id,]),
                    }
                ),
            ]),
            target_output,
            "Failed checking target dependencies."
        );
    }

    /// This test is intended to show how nested `[build-dependencies]` are
    /// successfully parsed and transitive dependencies are tracked (or more
    /// importantly for N+1 transitive deps, not tracked).
    #[test]
    fn parse_cargo_tree_output_nested_build_deps() {
        let autocfg_id = CrateId {
            name: "autocfg".to_owned(),
            version: Version::new(1, 3, 0),
        };
        let nested_build_dependencies_id = CrateId {
            name: "nested_build_dependencies".to_owned(),
            version: Version::new(0, 0, 0),
        };
        let num_traits_id = CrateId {
            name: "num-traits".to_owned(),
            version: Version::new(0, 2, 19),
        };
        let proc_macro2_id = CrateId {
            name: "proc-macro2".to_owned(),
            version: Version::new(1, 0, 86),
        };
        let proc_macro_error_attr_id = CrateId {
            name: "proc-macro-error-attr".to_owned(),
            version: Version::new(1, 0, 4),
        };
        let quote_id = CrateId {
            name: "quote".to_owned(),
            version: Version::new(1, 0, 37),
        };
        let syn_id = CrateId {
            name: "syn".to_owned(),
            version: Version::new(2, 0, 77),
        };
        let unicode_ident_id = CrateId {
            name: "unicode-ident".to_owned(),
            version: Version::new(1, 0, 12),
        };
        let version_check_id = CrateId {
            name: "version_check".to_owned(),
            version: Version::new(0, 9, 5),
        };

        let (target_output, host_output) = parse_cargo_tree_output(
        textwrap::dedent(
                r#"
                ;nested_build_dependencies v0.0.0 (/rules_rust/crate_universe/test_data/metadata/nested_build_dependencies);;
                [build-dependencies]
                |-- ;num-traits v0.2.19;default,std;
                |   [build-dependencies]
                |   `-- ;autocfg v1.3.0;;
                `-- ;syn v2.0.77;clone-impls,default,derive,parsing,printing,proc-macro;
                    |-- ;proc-macro2 v1.0.86;default,proc-macro;
                    |   `-- ;unicode-ident v1.0.12;;
                    |-- ;quote v1.0.37;default,proc-macro;
                    |   `-- ;proc-macro2 v1.0.86;default,proc-macro; (*)
                    `-- ;unicode-ident v1.0.12;;
                [dev-dependencies]
                `-- ;proc-macro-error-attr v1.0.4 (proc-macro);;
                    |-- ;proc-macro2 v1.0.86;default,proc-macro; (*)
                    `-- ;quote v1.0.37;default,proc-macro; (*)
                    [build-dependencies]
                    `-- ;version_check v0.9.5;;

                "#,
            ).lines().map(Ok::<&str, std::io::Error>),
        )
        .unwrap();

        assert_eq!(
            BTreeMap::from([
                (
                    TreeDepCompileKind::Host(autocfg_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::new(),
                    },
                ),
                (
                    TreeDepCompileKind::Host(num_traits_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "std".to_owned()]),
                        deps: BTreeSet::from([autocfg_id.clone(),]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(proc_macro_error_attr_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::from([
                            proc_macro2_id.clone(),
                            quote_id.clone(),
                            version_check_id.clone(),
                        ]),
                    }
                ),
                (
                    TreeDepCompileKind::Host(proc_macro2_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "proc-macro".to_owned()]),
                        deps: BTreeSet::from([unicode_ident_id.clone(),]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(quote_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from(["default".to_owned(), "proc-macro".to_owned()]),
                        deps: BTreeSet::from([proc_macro2_id.clone(),]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(syn_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::from([
                            "clone-impls".to_owned(),
                            "default".to_owned(),
                            "derive".to_owned(),
                            "parsing".to_owned(),
                            "printing".to_owned(),
                            "proc-macro".to_owned(),
                        ]),
                        deps: BTreeSet::from([
                            proc_macro2_id.clone(),
                            quote_id.clone(),
                            unicode_ident_id.clone()
                        ]),
                    },
                ),
                (
                    TreeDepCompileKind::Host(unicode_ident_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::new(),
                    }
                ),
                (
                    TreeDepCompileKind::Host(version_check_id.clone()),
                    CargoTreeEntry {
                        features: BTreeSet::new(),
                        deps: BTreeSet::new(),
                    }
                ),
            ]),
            host_output,
            "Failed checking host dependencies."
        );

        assert_eq!(
            BTreeMap::from([(
                TreeDepCompileKind::Target(nested_build_dependencies_id.clone()),
                CargoTreeEntry {
                    features: BTreeSet::new(),
                    deps: BTreeSet::from([
                        num_traits_id.clone(),
                        syn_id.clone(),
                        proc_macro_error_attr_id.clone(),
                    ]),
                }
            )]),
            target_output,
            "Failed checking target dependencies."
        );
    }

    #[test]
    fn parse_cargo_tree_output_same_level_host_deps() {
        let autocfg_id = CrateId {
            name: "autocfg".to_owned(),
            version: Version::new(1, 2, 0),
        };

        let (_target_output, host_output) = parse_cargo_tree_output(
        textwrap::dedent(
                // This is a modified/reduced output of the `resolver_2_deps` manifest.
                r#"
                ;resolver_2_deps v0.1.0 (/rules_rust/crate_universe/test_data/metadata/resolver_2_deps);;
                |-- ;reqwest v0.12.4;__tls,charset,default,default-tls,h2,http2,macos-system-configuration;
                |   |-- ;h2 v0.4.4;;
                |   |   |-- ;slab v0.4.9;default,std;
                |   |   |   [build-dependencies]
                |   |   |   `-- ;autocfg v1.2.0;;
                |   |   |-- ;tokio v1.37.0;bytes,default,io-std,io-util,libc,mio,net,rt,socket2,sync,time;
                |   |   |   |-- ;bytes v1.6.0;default,std;
                |   |   |   |-- ;libc v0.2.153;default,std;
                |   |   |   |-- ;mio v0.8.11;net,os-ext,os-poll;
                |   |   |   |   `-- ;libc v0.2.153;default,std;
                |   |   |   |-- ;pin-project-lite v0.2.14;;
                |   |   |   `-- ;socket2 v0.5.7;all;
                |   |   |       `-- ;libc v0.2.153;default,std;
                "#,
            ).lines().map(Ok::<&str, std::io::Error>),
        )
        .unwrap();

        assert_eq!(
            BTreeMap::from([(
                TreeDepCompileKind::Host(autocfg_id.clone()),
                CargoTreeEntry {
                    features: BTreeSet::new(),
                    deps: BTreeSet::new(),
                },
            ),]),
            host_output,
            "Failed checking host dependencies."
        );
    }
}
