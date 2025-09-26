//! A module for configuration information

use std::cmp::Ordering;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Formatter;
use std::iter::Sum;
use std::ops::Add;
use std::path::Path;
use std::str::FromStr;
use std::{fmt, fs};

use anyhow::{Context, Result};
use cargo_lock::package::GitReference;
use cargo_metadata::Package;
use semver::VersionReq;
use serde::de::value::SeqAccessDeserializer;
use serde::de::{Deserializer, SeqAccess, Unexpected, Visitor};
use serde::{Deserialize, Serialize, Serializer};

use crate::select::{Select, Selectable};
use crate::utils::starlark::Label;
use crate::utils::target_triple::TargetTriple;

/// Representations of different kinds of crate vendoring into workspaces.
#[derive(Debug, Serialize, Deserialize, Clone, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub(crate) enum VendorMode {
    /// Crates having full source being vendored into a workspace
    Local,

    /// Crates having only BUILD files with repository rules vendored into a workspace
    Remote,
}

impl std::fmt::Display for VendorMode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(
            match self {
                VendorMode::Local => "local",
                VendorMode::Remote => "remote",
            },
            f,
        )
    }
}

#[derive(Debug, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub(crate) struct RenderConfig {
    /// The name of the repository being rendered
    pub(crate) repository_name: String,

    /// The pattern to use for BUILD file names.
    /// Eg. `//:BUILD.{name}-{version}.bazel`
    #[serde(default = "default_build_file_template")]
    pub(crate) build_file_template: String,

    /// The pattern to use for a crate target.
    /// Eg. `@{repository}__{name}-{version}//:{target}`
    #[serde(default = "default_crate_label_template")]
    pub(crate) crate_label_template: String,

    /// The pattern to use for the `defs.bzl` and `BUILD.bazel`
    /// file names used for the crates module.
    /// Eg. `//:{file}`
    #[serde(default = "default_crates_module_template")]
    pub(crate) crates_module_template: String,

    /// The pattern used for a crate's repository name.
    /// Eg. `{repository}__{name}-{version}`
    #[serde(default = "default_crate_repository_template")]
    pub(crate) crate_repository_template: String,

    /// Default alias rule to use for packages.  Can be overridden by annotations.
    #[serde(default)]
    pub(crate) default_alias_rule: AliasRule,

    /// The default of the `package_name` parameter to use for the module macros like `all_crate_deps`.
    /// In general, this should be be unset to allow the macros to do auto-detection in the analysis phase.
    pub(crate) default_package_name: Option<String>,

    /// Whether to generate `target_compatible_with` annotations on the generated BUILD files.  This
    /// catches a `target_triple`being targeted that isn't declared in `supported_platform_triples`.
    #[serde(default = "default_generate_target_compatible_with")]
    pub(crate) generate_target_compatible_with: bool,

    /// The pattern to use for platform constraints.
    /// Eg. `@rules_rust//rust/platform:{triple}`.
    #[serde(default = "default_platforms_template")]
    pub(crate) platforms_template: String,

    /// The command to use for regenerating generated files.
    pub(crate) regen_command: String,

    /// An optional configuration for rendering content to be rendered into repositories.
    pub(crate) vendor_mode: Option<VendorMode>,

    /// Whether to generate package metadata
    #[serde(default = "default_generate_rules_license_metadata")]
    pub(crate) generate_rules_license_metadata: bool,
}

// Default is manually implemented so that the default values match the default
// values when deserializing, which involves calling the vairous `default_x()`
// functions specified in `#[serde(default = "default_x")]`.
impl Default for RenderConfig {
    fn default() -> Self {
        RenderConfig {
            repository_name: String::default(),
            build_file_template: default_build_file_template(),
            crate_label_template: default_crate_label_template(),
            crates_module_template: default_crates_module_template(),
            crate_repository_template: default_crate_repository_template(),
            default_alias_rule: AliasRule::default(),
            default_package_name: Option::default(),
            generate_target_compatible_with: default_generate_target_compatible_with(),
            platforms_template: default_platforms_template(),
            regen_command: String::default(),
            vendor_mode: Option::default(),
            generate_rules_license_metadata: default_generate_rules_license_metadata(),
        }
    }
}

impl RenderConfig {
    pub(crate) fn are_sources_present(&self) -> bool {
        self.vendor_mode == Some(VendorMode::Local)
    }
}

fn default_build_file_template() -> String {
    "//:BUILD.{name}-{version}.bazel".to_owned()
}

fn default_crates_module_template() -> String {
    "//:{file}".to_owned()
}

fn default_crate_label_template() -> String {
    "@{repository}__{name}-{version}//:{target}".to_owned()
}

fn default_crate_repository_template() -> String {
    "{repository}__{name}-{version}".to_owned()
}

fn default_platforms_template() -> String {
    "@rules_rust//rust/platform:{triple}".to_owned()
}

fn default_generate_target_compatible_with() -> bool {
    true
}

fn default_generate_rules_license_metadata() -> bool {
    false
}

/// A representation of some Git identifier used to represent the "revision" or "pin" of a checkout.
#[derive(Debug, Serialize, Deserialize, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum Commitish {
    /// From a tag.
    Tag(String),

    /// From the HEAD of a branch.
    Branch(String),

    /// From a specific revision.
    Rev(String),
}

impl From<GitReference> for Commitish {
    fn from(git_ref: GitReference) -> Self {
        match git_ref {
            GitReference::Tag(v) => Self::Tag(v),
            GitReference::Branch(v) => Self::Branch(v),
            GitReference::Rev(v) => Self::Rev(v),
        }
    }
}

#[derive(Debug, Default, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Clone)]
pub(crate) enum AliasRule {
    #[default]
    #[serde(rename = "alias")]
    Alias,
    #[serde(rename = "dbg")]
    Dbg,
    #[serde(rename = "fastbuild")]
    Fastbuild,
    #[serde(rename = "opt")]
    Opt,
    #[serde(untagged)]
    Custom { bzl: String, rule: String },
}

impl AliasRule {
    pub(crate) fn bzl(&self) -> Option<String> {
        match self {
            AliasRule::Alias => None,
            AliasRule::Dbg | AliasRule::Fastbuild | AliasRule::Opt => {
                Some("//:alias_rules.bzl".to_owned())
            }
            AliasRule::Custom { bzl, .. } => Some(bzl.clone()),
        }
    }

    pub(crate) fn rule(&self) -> String {
        match self {
            AliasRule::Alias => "alias".to_owned(),
            AliasRule::Dbg => "transition_alias_dbg".to_owned(),
            AliasRule::Fastbuild => "transition_alias_fastbuild".to_owned(),
            AliasRule::Opt => "transition_alias_opt".to_owned(),
            AliasRule::Custom { rule, .. } => rule.clone(),
        }
    }
}

#[derive(Debug, Default, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) struct CrateAnnotations {
    /// Which subset of the crate's bins should get produced as `rust_binary` targets.
    pub(crate) gen_binaries: Option<GenBinaries>,

    /// Determins whether or not Cargo build scripts should be generated for the current package
    pub(crate) gen_build_script: Option<bool>,

    /// Additional data to pass to
    /// [deps](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-deps) attribute.
    pub(crate) deps: Option<Select<BTreeSet<Label>>>,

    /// Additional data to pass to
    /// [proc_macro_deps](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-proc_macro_deps) attribute.
    pub(crate) proc_macro_deps: Option<Select<BTreeSet<Label>>>,

    /// Additional data to pass to  the target's
    /// [crate_features](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-crate_features) attribute.
    pub(crate) crate_features: Option<Select<BTreeSet<String>>>,

    /// Additional data to pass to  the target's
    /// [data](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-data) attribute.
    pub(crate) data: Option<Select<BTreeSet<Label>>>,

    /// An optional glob pattern to set on the
    /// [data](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-data) attribute.
    pub(crate) data_glob: Option<BTreeSet<String>>,

    /// Additional data to pass to
    /// [compile_data](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-compile_data) attribute.
    pub(crate) compile_data: Option<Select<BTreeSet<Label>>>,

    /// An optional glob pattern to set on the
    /// [compile_data](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-compile_data) attribute.
    pub(crate) compile_data_glob: Option<BTreeSet<String>>,

    /// If true, disables pipelining for library targets generated for this crate.
    pub(crate) disable_pipelining: bool,

    /// Additional data to pass to  the target's
    /// [rustc_env](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-rustc_env) attribute.
    pub(crate) rustc_env: Option<Select<BTreeMap<String, String>>>,

    /// Additional data to pass to  the target's
    /// [rustc_env_files](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-rustc_env_files) attribute.
    pub(crate) rustc_env_files: Option<Select<BTreeSet<String>>>,

    /// Additional data to pass to the target's
    /// [rustc_flags](https://bazelbuild.github.io/rules_rust/defs.html#rust_library-rustc_flags) attribute.
    pub(crate) rustc_flags: Option<Select<Vec<String>>>,

    /// Additional dependencies to pass to a build script's
    /// [deps](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-deps) attribute.
    pub(crate) build_script_deps: Option<Select<BTreeSet<Label>>>,

    /// Additional data to pass to a build script's
    /// [proc_macro_deps](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-proc_macro_deps) attribute.
    pub(crate) build_script_proc_macro_deps: Option<Select<BTreeSet<Label>>>,

    /// Additional compile-only data to pass to a build script's
    /// [compile_data](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-compile_data) attribute.
    pub(crate) build_script_compile_data: Option<Select<BTreeSet<Label>>>,

    /// Additional data to pass to a build script's
    /// [build_script_data](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-data) attribute.
    pub(crate) build_script_data: Option<Select<BTreeSet<Label>>>,

    /// Additional data to pass to a build script's
    /// [tools](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-tools) attribute.
    pub(crate) build_script_tools: Option<Select<BTreeSet<Label>>>,

    /// An optional glob pattern to set on the
    /// [build_script_data](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-build_script_env) attribute.
    pub(crate) build_script_data_glob: Option<BTreeSet<String>>,

    /// Additional environment variables to pass to a build script's
    /// [build_script_env](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-rustc_env) attribute.
    pub(crate) build_script_env: Option<Select<BTreeMap<String, String>>>,

    /// Additional rustc_env flags to pass to a build script's
    /// [rustc_env](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-rustc_env) attribute.
    pub(crate) build_script_rustc_env: Option<Select<BTreeMap<String, String>>>,

    /// Additional labels to pass to a build script's
    /// [toolchains](https://bazel.build/reference/be/common-definitions#common-attributes) attribute.
    pub(crate) build_script_toolchains: Option<BTreeSet<Label>>,

    /// Additional rustc_env flags to pass to a build script's
    /// [use_default_shell_env](https://bazelbuild.github.io/rules_rust/cargo.html#cargo_build_script-use_default_shell_env) attribute.
    pub(crate) build_script_use_default_shell_env: Option<i32>,

    /// Directory to run the crate's build script in. If not set, will run in the manifest directory, otherwise a directory relative to the exec root.
    pub(crate) build_script_rundir: Option<Select<String>>,

    /// A scratch pad used to write arbitrary text to target BUILD files.
    pub(crate) additive_build_file_content: Option<String>,

    /// For git sourced crates, this is a the
    /// [git_repository::shallow_since](https://docs.bazel.build/versions/main/repo/git.html#new_git_repository-shallow_since) attribute.
    pub(crate) shallow_since: Option<String>,

    /// The `patch_args` attribute of a Bazel repository rule. See
    /// [http_archive.patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)
    pub(crate) patch_args: Option<Vec<String>>,

    /// The `patch_tool` attribute of a Bazel repository rule. See
    /// [http_archive.patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)
    pub(crate) patch_tool: Option<String>,

    /// The `patches` attribute of a Bazel repository rule. See
    /// [http_archive.patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)
    pub(crate) patches: Option<BTreeSet<String>>,

    /// Extra targets the should be aliased during rendering.
    pub(crate) extra_aliased_targets: Option<BTreeMap<String, String>>,

    /// Transition rule to use instead of `native.alias()`.
    pub(crate) alias_rule: Option<AliasRule>,

    /// The crates to use instead of the generated one.
    pub(crate) override_targets: Option<BTreeMap<String, Label>>,
}

macro_rules! joined_extra_member {
    ($lhs:expr, $rhs:expr, $fn_new:expr, $fn_extend:expr) => {
        if let Some(lhs) = $lhs {
            if let Some(rhs) = $rhs {
                let mut new = $fn_new();
                $fn_extend(&mut new, lhs);
                $fn_extend(&mut new, rhs);
                Some(new)
            } else {
                Some(lhs)
            }
        } else if $rhs.is_some() {
            $rhs
        } else {
            None
        }
    };
}

impl Add for CrateAnnotations {
    type Output = CrateAnnotations;

    fn add(self, rhs: Self) -> Self::Output {
        fn select_merge<T>(lhs: Option<Select<T>>, rhs: Option<Select<T>>) -> Option<Select<T>>
        where
            T: Selectable,
        {
            match (lhs, rhs) {
                (Some(lhs), Some(rhs)) => Some(Select::merge(lhs, rhs)),
                (Some(lhs), None) => Some(lhs),
                (None, Some(rhs)) => Some(rhs),
                (None, None) => None,
            }
        }

        let concat_string = |lhs: &mut String, rhs: String| {
            *lhs = format!("{lhs}{rhs}");
        };

        #[rustfmt::skip]
        let output = CrateAnnotations {
            gen_binaries: self.gen_binaries.or(rhs.gen_binaries),
            gen_build_script: self.gen_build_script.or(rhs.gen_build_script),
            deps: select_merge(self.deps, rhs.deps),
            proc_macro_deps: select_merge(self.proc_macro_deps, rhs.proc_macro_deps),
            crate_features: select_merge(self.crate_features, rhs.crate_features),
            data: select_merge(self.data, rhs.data),
            data_glob: joined_extra_member!(self.data_glob, rhs.data_glob, BTreeSet::new, BTreeSet::extend),
            disable_pipelining: self.disable_pipelining || rhs.disable_pipelining,
            compile_data: select_merge(self.compile_data, rhs.compile_data),
            compile_data_glob: joined_extra_member!(self.compile_data_glob, rhs.compile_data_glob, BTreeSet::new, BTreeSet::extend),
            rustc_env: select_merge(self.rustc_env, rhs.rustc_env),
            rustc_env_files: select_merge(self.rustc_env_files, rhs.rustc_env_files),
            rustc_flags: select_merge(self.rustc_flags, rhs.rustc_flags),
            build_script_deps: select_merge(self.build_script_deps, rhs.build_script_deps),
            build_script_proc_macro_deps: select_merge(self.build_script_proc_macro_deps, rhs.build_script_proc_macro_deps),
            build_script_compile_data: select_merge(self.build_script_compile_data, rhs.build_script_compile_data),
            build_script_data: select_merge(self.build_script_data, rhs.build_script_data),
            build_script_tools: select_merge(self.build_script_tools, rhs.build_script_tools),
            build_script_data_glob: joined_extra_member!(self.build_script_data_glob, rhs.build_script_data_glob, BTreeSet::new, BTreeSet::extend),
            build_script_env: select_merge(self.build_script_env, rhs.build_script_env),
            build_script_rustc_env: select_merge(self.build_script_rustc_env, rhs.build_script_rustc_env),
            build_script_toolchains: joined_extra_member!(self.build_script_toolchains, rhs.build_script_toolchains, BTreeSet::new, BTreeSet::extend),
            build_script_use_default_shell_env: self.build_script_use_default_shell_env.or(rhs.build_script_use_default_shell_env),
            build_script_rundir: self.build_script_rundir.or(rhs.build_script_rundir),
            additive_build_file_content: joined_extra_member!(self.additive_build_file_content, rhs.additive_build_file_content, String::new, concat_string),
            shallow_since: self.shallow_since.or(rhs.shallow_since),
            patch_args: joined_extra_member!(self.patch_args, rhs.patch_args, Vec::new, Vec::extend),
            patch_tool: self.patch_tool.or(rhs.patch_tool),
            patches: joined_extra_member!(self.patches, rhs.patches, BTreeSet::new, BTreeSet::extend),
            extra_aliased_targets: joined_extra_member!(self.extra_aliased_targets, rhs.extra_aliased_targets, BTreeMap::new, BTreeMap::extend),
            alias_rule: self.alias_rule.or(rhs.alias_rule),
            override_targets: self.override_targets.or(rhs.override_targets),
        };

        output
    }
}

impl Sum for CrateAnnotations {
    fn sum<I: Iterator<Item = Self>>(iter: I) -> Self {
        iter.fold(CrateAnnotations::default(), |a, b| a + b)
    }
}

/// A subset of `crate.annotation` that we allow packages to define in their
/// free-form Cargo.toml metadata.
///
/// ```toml
/// [package.metadata.bazel]
/// additive_build_file_contents = """
///     ...
/// """
/// data = ["font.woff2"]
/// extra_aliased_targets = { ... }
/// gen_build_script = false
/// ```
///
/// These are considered default values which apply if the Bazel workspace does
/// not specify a different value for the same annotation in their
/// crates_repository attributes.
#[derive(Debug, Deserialize)]
pub(crate) struct AnnotationsProvidedByPackage {
    pub(crate) gen_build_script: Option<bool>,
    pub(crate) data: Option<Select<BTreeSet<Label>>>,
    pub(crate) data_glob: Option<BTreeSet<String>>,
    pub(crate) deps: Option<Select<BTreeSet<Label>>>,
    pub(crate) compile_data: Option<Select<BTreeSet<Label>>>,
    pub(crate) compile_data_glob: Option<BTreeSet<String>>,
    pub(crate) rustc_env: Option<Select<BTreeMap<String, String>>>,
    pub(crate) rustc_env_files: Option<Select<BTreeSet<String>>>,
    pub(crate) rustc_flags: Option<Select<Vec<String>>>,
    pub(crate) build_script_env: Option<Select<BTreeMap<String, String>>>,
    pub(crate) build_script_rustc_env: Option<Select<BTreeMap<String, String>>>,
    pub(crate) build_script_rundir: Option<Select<String>>,
    pub(crate) additive_build_file_content: Option<String>,
    pub(crate) extra_aliased_targets: Option<BTreeMap<String, String>>,
}

impl CrateAnnotations {
    pub(crate) fn apply_defaults_from_package_metadata(
        &mut self,
        pkg_metadata: &serde_json::Value,
    ) {
        #[deny(unused_variables)]
        let AnnotationsProvidedByPackage {
            gen_build_script,
            data,
            data_glob,
            deps,
            compile_data,
            compile_data_glob,
            rustc_env,
            rustc_env_files,
            rustc_flags,
            build_script_env,
            build_script_rustc_env,
            build_script_rundir,
            additive_build_file_content,
            extra_aliased_targets,
        } = match AnnotationsProvidedByPackage::deserialize(&pkg_metadata["bazel"]) {
            Ok(annotations) => annotations,
            // Ignore bad annotations. The set of supported annotations evolves
            // over time across different versions of crate_universe, and we
            // don't want a library to be impossible to import into Bazel for
            // having old or broken annotations. The Bazel workspace can specify
            // its own correct annotations.
            Err(_) => return,
        };

        fn default<T>(workspace_value: &mut Option<T>, default_value: Option<T>) {
            if workspace_value.is_none() {
                *workspace_value = default_value;
            }
        }

        default(&mut self.gen_build_script, gen_build_script);
        default(&mut self.gen_build_script, gen_build_script);
        default(&mut self.data, data);
        default(&mut self.data_glob, data_glob);
        default(&mut self.deps, deps);
        default(&mut self.compile_data, compile_data);
        default(&mut self.compile_data_glob, compile_data_glob);
        default(&mut self.rustc_env, rustc_env);
        default(&mut self.rustc_env_files, rustc_env_files);
        default(&mut self.rustc_flags, rustc_flags);
        default(&mut self.build_script_env, build_script_env);
        default(&mut self.build_script_rustc_env, build_script_rustc_env);
        default(&mut self.build_script_rundir, build_script_rundir);
        default(
            &mut self.additive_build_file_content,
            additive_build_file_content,
        );
        default(&mut self.extra_aliased_targets, extra_aliased_targets);
    }
}

/// A unique identifier for Crates
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct CrateId {
    /// The name of the crate
    pub name: String,

    /// The crate's semantic version
    pub version: semver::Version,
}

impl CrateId {
    /// Construct a new [CrateId]
    pub(crate) fn new(name: String, version: semver::Version) -> Self {
        Self { name, version }
    }
}

impl From<&Package> for CrateId {
    fn from(package: &Package) -> Self {
        Self {
            name: package.name.clone(),
            version: package.version.clone(),
        }
    }
}

impl Serialize for CrateId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&format!("{} {}", self.name, self.version))
    }
}

struct CrateIdVisitor;
impl Visitor<'_> for CrateIdVisitor {
    type Value = CrateId;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("Expected string value of `{name} {version}`.")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let (name, version_str) = v.rsplit_once(' ').ok_or_else(|| {
            E::custom(format!(
                "Expected string value of `{{name}} {{version}}`. Got '{v}'"
            ))
        })?;
        let version = semver::Version::parse(version_str).map_err(|err| {
            E::custom(format!(
                "Couldn't parse {version_str} as a semver::Version: {err}"
            ))
        })?;
        Ok(CrateId {
            name: name.to_string(),
            version,
        })
    }
}

impl<'de> Deserialize<'de> for CrateId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_str(CrateIdVisitor)
    }
}

impl std::fmt::Display for CrateId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&format!("{} {}", self.name, self.version), f)
    }
}

#[derive(Debug, Hash, Clone, PartialEq, Eq)]
pub(crate) enum GenBinaries {
    All,
    Some(BTreeSet<String>),
}

impl Default for GenBinaries {
    fn default() -> Self {
        GenBinaries::Some(BTreeSet::new())
    }
}

impl Serialize for GenBinaries {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            GenBinaries::All => serializer.serialize_bool(true),
            GenBinaries::Some(set) if set.is_empty() => serializer.serialize_bool(false),
            GenBinaries::Some(set) => serializer.collect_seq(set),
        }
    }
}

impl<'de> Deserialize<'de> for GenBinaries {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(GenBinariesVisitor)
    }
}

struct GenBinariesVisitor;
impl<'de> Visitor<'de> for GenBinariesVisitor {
    type Value = GenBinaries;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("boolean, or array of bin names")
    }

    fn visit_bool<E>(self, gen_binaries: bool) -> Result<Self::Value, E> {
        if gen_binaries {
            Ok(GenBinaries::All)
        } else {
            Ok(GenBinaries::Some(BTreeSet::new()))
        }
    }

    fn visit_seq<A>(self, seq: A) -> Result<Self::Value, A::Error>
    where
        A: SeqAccess<'de>,
    {
        BTreeSet::deserialize(SeqAccessDeserializer::new(seq)).map(GenBinaries::Some)
    }
}

/// Workspace specific settings to control how targets are generated
#[derive(Debug, Default, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub(crate) struct Config {
    /// Whether to generate `rust_binary` targets for all bins by default
    pub(crate) generate_binaries: bool,

    /// Whether or not to generate Cargo build scripts by default
    pub(crate) generate_build_scripts: bool,

    /// Additional settings to apply to generated crates
    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub(crate) annotations: BTreeMap<CrateNameAndVersionReq, CrateAnnotations>,

    /// Settings used to determine various render info
    pub(crate) rendering: RenderConfig,

    /// The contents of a Cargo configuration file
    pub(crate) cargo_config: Option<toml::Value>,

    /// A set of platform triples to use in generated select statements
    #[serde(default, skip_serializing_if = "BTreeSet::is_empty")]
    pub(crate) supported_platform_triples: BTreeSet<TargetTriple>,
}

impl Config {
    pub(crate) fn try_from_path<T: AsRef<Path>>(path: T) -> Result<Self> {
        let data = fs::read_to_string(path)?;
        Ok(serde_json::from_str(&data)?)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct CrateNameAndVersionReq {
    /// The name of the crate
    pub name: String,

    version_req_string: VersionReqString,
}

impl Serialize for CrateNameAndVersionReq {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&format!(
            "{} {}",
            self.name, self.version_req_string.original
        ))
    }
}

struct CrateNameAndVersionReqVisitor;
impl Visitor<'_> for CrateNameAndVersionReqVisitor {
    type Value = CrateNameAndVersionReq;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("Expected string value of `{name} {version}`.")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let (name, version) = v.rsplit_once(' ').ok_or_else(|| {
            E::custom(format!(
                "Expected string value of `{{name}} {{version}}`. Got '{v}'"
            ))
        })?;
        version
            .parse()
            .map(|version| CrateNameAndVersionReq {
                name: name.to_string(),
                version_req_string: version,
            })
            .map_err(|err| E::custom(err.to_string()))
    }
}

impl<'de> Deserialize<'de> for CrateNameAndVersionReq {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_str(CrateNameAndVersionReqVisitor)
    }
}

/// A version requirement (i.e. a semver::VersionReq) which preserves the original string it was parsed from.
/// This means that you can report back to the user whether they wrote `1` or `1.0.0` or `^1.0.0` or `>=1,<2`,
/// and support exact round-trip serialization and deserialization.
#[derive(Clone, Debug)]
pub struct VersionReqString {
    original: String,

    parsed: VersionReq,
}

impl FromStr for VersionReqString {
    type Err = anyhow::Error;

    fn from_str(original: &str) -> Result<Self, Self::Err> {
        let parsed = VersionReq::parse(original)
            .context("VersionReqString must be a valid semver requirement")?;
        Ok(VersionReqString {
            original: original.to_owned(),
            parsed,
        })
    }
}

impl PartialEq for VersionReqString {
    fn eq(&self, other: &Self) -> bool {
        self.original == other.original
    }
}

impl Eq for VersionReqString {}

impl PartialOrd for VersionReqString {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for VersionReqString {
    fn cmp(&self, other: &Self) -> Ordering {
        Ord::cmp(&self.original, &other.original)
    }
}

impl Serialize for VersionReqString {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.original)
    }
}

impl<'de> Deserialize<'de> for VersionReqString {
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct StringVisitor;

        impl Visitor<'_> for StringVisitor {
            type Value = String;

            fn expecting(&self, formatter: &mut Formatter) -> fmt::Result {
                formatter.write_str("string of a semver requirement")
            }
        }

        let original = deserializer.deserialize_str(StringVisitor)?;
        let parsed = VersionReq::parse(&original).map_err(|_| {
            serde::de::Error::invalid_value(
                Unexpected::Str(&original),
                &"a valid semver requirement",
            )
        })?;
        Ok(VersionReqString { original, parsed })
    }
}

impl CrateNameAndVersionReq {
    #[cfg(test)]
    pub fn new(name: String, version_req_string: VersionReqString) -> CrateNameAndVersionReq {
        CrateNameAndVersionReq {
            name,
            version_req_string,
        }
    }

    /// Compares a [CrateNameAndVersionReq] against a [cargo_metadata::Package].
    pub fn matches(&self, package: &Package) -> bool {
        // If the package name does not match, it's obviously
        // not the right package
        if self.name != "*" && self.name != package.name {
            return false;
        }

        // First see if the package version matches exactly
        if package.version.to_string() == self.version_req_string.original {
            return true;
        }

        // If the version provided is the wildcard "*", it matches. Do not
        // delegate to the semver crate in this case because semver does not
        // consider "*" to match prerelease packages. That's expected behavior
        // in the context of declaring package dependencies, but not in the
        // context of declaring which versions of preselected packages an
        // annotation applies to.
        if self.version_req_string.original == "*" {
            return true;
        }

        // Next, check to see if the version provided is a semver req and
        // check if the package matches the condition
        self.version_req_string.parsed.matches(&package.version)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use crate::test::*;

    #[test]
    fn test_crate_id_serde() {
        let id: CrateId = serde_json::from_str("\"crate 0.1.0\"").unwrap();
        assert_eq!(
            id,
            CrateId::new("crate".to_owned(), semver::Version::new(0, 1, 0))
        );
        assert_eq!(serde_json::to_string(&id).unwrap(), "\"crate 0.1.0\"");
    }

    #[test]
    fn test_crate_id_matches() {
        let mut package = mock_cargo_metadata_package();
        let id = CrateNameAndVersionReq::new("mock-pkg".to_owned(), "0.1.0".parse().unwrap());

        package.version = cargo_metadata::semver::Version::new(0, 1, 0);
        assert!(id.matches(&package));

        package.version = cargo_metadata::semver::Version::new(1, 0, 0);
        assert!(!id.matches(&package));
    }

    #[test]
    fn test_crate_name_and_version_req_serde() {
        let id: CrateNameAndVersionReq = serde_json::from_str("\"crate 0.1.0\"").unwrap();
        assert_eq!(
            id,
            CrateNameAndVersionReq::new(
                "crate".to_owned(),
                VersionReqString::from_str("0.1.0").unwrap()
            )
        );
        assert_eq!(serde_json::to_string(&id).unwrap(), "\"crate 0.1.0\"");
    }

    #[test]
    fn test_crate_name_and_version_req_serde_semver() {
        let id: CrateNameAndVersionReq = serde_json::from_str("\"crate *\"").unwrap();
        assert_eq!(
            id,
            CrateNameAndVersionReq::new(
                "crate".to_owned(),
                VersionReqString::from_str("*").unwrap()
            )
        );
        assert_eq!(serde_json::to_string(&id).unwrap(), "\"crate *\"");
    }

    #[test]
    fn test_crate_name_and_version_req_semver_matches() {
        let mut package = mock_cargo_metadata_package();
        package.version = cargo_metadata::semver::Version::new(1, 0, 0);
        let id = CrateNameAndVersionReq::new("mock-pkg".to_owned(), "*".parse().unwrap());
        assert!(id.matches(&package));

        let mut prerelease = mock_cargo_metadata_package();
        prerelease.version = cargo_metadata::semver::Version::parse("1.0.0-pre.0").unwrap();
        assert!(id.matches(&prerelease));

        let id = CrateNameAndVersionReq::new("mock-pkg".to_owned(), "<1".parse().unwrap());
        assert!(!id.matches(&package));
    }

    #[test]
    fn deserialize_config() {
        let runfiles = runfiles::Runfiles::create().unwrap();
        let path = runfiles::rlocation!(
            runfiles,
            "rules_rust/crate_universe/test_data/serialized_configs/config.json"
        )
        .unwrap();

        let content = std::fs::read_to_string(path).unwrap();

        let config: Config = serde_json::from_str(&content).unwrap();

        // Annotations
        let annotation = config
            .annotations
            .get(&CrateNameAndVersionReq::new(
                "rand".to_owned(),
                "0.8.5".parse().unwrap(),
            ))
            .unwrap();
        assert_eq!(
            annotation.crate_features,
            Some(Select::from_value(BTreeSet::from(["small_rng".to_owned()])))
        );

        // Global settings
        assert!(config.cargo_config.is_none());
        assert!(!config.generate_binaries);
        assert!(!config.generate_build_scripts);

        // Render Config
        assert_eq!(
            config.rendering.platforms_template,
            "//custom/platform:{triple}"
        );
    }
}
