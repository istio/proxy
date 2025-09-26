//! A module for representations of starlark constructs

mod glob;
mod label;
mod select;
mod select_dict;
mod select_list;
mod select_scalar;
mod select_set;
mod serialize;
mod target_compatible_with;

use std::collections::BTreeSet as Set;

use serde::{Serialize, Serializer};
use serde_starlark::{Error as StarlarkError, FunctionCall};

pub(crate) use glob::*;
pub(crate) use label::*;
pub(crate) use select::*;
pub(crate) use select_dict::*;
pub(crate) use select_list::*;
pub(crate) use select_scalar::*;
pub(crate) use select_set::*;
pub(crate) use target_compatible_with::*;

#[derive(Serialize)]
#[serde(untagged)]
pub(crate) enum Starlark {
    Load(Load),
    Package(Package),
    PackageInfo(PackageInfo),
    License(License),
    ExportsFiles(ExportsFiles),
    Filegroup(Filegroup),
    Alias(Alias),
    CargoBuildScript(CargoBuildScript),
    #[serde(serialize_with = "serialize::rust_proc_macro")]
    RustProcMacro(RustProcMacro),
    #[serde(serialize_with = "serialize::rust_library")]
    RustLibrary(RustLibrary),
    #[serde(serialize_with = "serialize::rust_binary")]
    RustBinary(RustBinary),

    #[serde(skip_serializing)]
    Verbatim(String),
}

pub(crate) struct Load {
    pub(crate) bzl: String,
    pub(crate) items: Set<String>,
}

pub(crate) struct Package {
    pub(crate) default_package_metadata: Set<Label>,
    pub(crate) default_visibility: Set<String>,
}

pub(crate) struct PackageInfo {
    pub(crate) name: String,
    pub(crate) package_name: String,
    pub(crate) package_url: String,
    pub(crate) package_version: String,
}

pub(crate) struct License {
    pub(crate) name: String,
    pub(crate) license_kinds: Set<String>,
    pub(crate) license_text: String,
}

pub(crate) struct ExportsFiles {
    pub(crate) paths: Set<String>,
    pub(crate) globs: Glob,
}

#[derive(Serialize)]
#[serde(rename = "filegroup")]
pub(crate) struct Filegroup {
    pub(crate) name: String,
    pub(crate) srcs: Glob,
}

pub(crate) struct Alias {
    pub(crate) rule: String,
    pub(crate) name: String,
    pub(crate) actual: Label,
    pub(crate) tags: Set<String>,
}

#[derive(Debug, Serialize)]
#[serde(rename = "cargo_build_script")]
pub(crate) struct CargoBuildScript {
    pub(crate) name: String,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) aliases: SelectDict<Label, String>,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) build_script_env: SelectDict<String, String>,
    #[serde(skip_serializing_if = "Data::is_empty")]
    pub(crate) compile_data: Data,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) crate_features: SelectSet<String>,
    pub(crate) crate_name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) crate_root: Option<String>,
    #[serde(skip_serializing_if = "Data::is_empty")]
    pub(crate) data: Data,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) link_deps: SelectSet<Label>,
    pub(crate) edition: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) linker_script: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) links: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) pkg_name: Option<String>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) proc_macro_deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectScalar::is_empty")]
    pub(crate) rundir: SelectScalar<String>,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) rustc_env: SelectDict<String, String>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) rustc_env_files: SelectSet<String>,
    #[serde(skip_serializing_if = "SelectList::is_empty")]
    pub(crate) rustc_flags: SelectList<String>,
    pub(crate) srcs: Glob,
    #[serde(skip_serializing_if = "Set::is_empty")]
    pub(crate) tags: Set<String>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) tools: SelectSet<Label>,
    #[serde(skip_serializing_if = "Set::is_empty")]
    pub(crate) toolchains: Set<Label>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) use_default_shell_env: Option<i32>,
    pub(crate) version: String,
    pub(crate) visibility: Set<String>,
}

#[derive(Serialize)]
pub(crate) struct RustProcMacro {
    pub(crate) name: String,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) proc_macro_deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) aliases: SelectDict<Label, String>,
    #[serde(flatten)]
    pub(crate) common: CommonAttrs,
}

#[derive(Serialize)]
pub(crate) struct RustLibrary {
    pub(crate) name: String,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) proc_macro_deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) aliases: SelectDict<Label, String>,
    #[serde(flatten)]
    pub(crate) common: CommonAttrs,
    #[serde(skip_serializing_if = "std::ops::Not::not")]
    pub(crate) disable_pipelining: bool,
}

#[derive(Serialize)]
pub(crate) struct RustBinary {
    pub(crate) name: String,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) proc_macro_deps: SelectSet<Label>,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) aliases: SelectDict<Label, String>,
    #[serde(flatten)]
    pub(crate) common: CommonAttrs,
}

#[derive(Serialize)]
pub(crate) struct CommonAttrs {
    #[serde(skip_serializing_if = "Data::is_empty")]
    pub(crate) compile_data: Data,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) crate_features: SelectSet<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) crate_root: Option<String>,
    #[serde(skip_serializing_if = "Data::is_empty")]
    pub(crate) data: Data,
    pub(crate) edition: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) linker_script: Option<String>,
    #[serde(skip_serializing_if = "SelectDict::is_empty")]
    pub(crate) rustc_env: SelectDict<String, String>,
    #[serde(skip_serializing_if = "SelectSet::is_empty")]
    pub(crate) rustc_env_files: SelectSet<String>,
    #[serde(skip_serializing_if = "SelectList::is_empty")]
    pub(crate) rustc_flags: SelectList<String>,
    pub(crate) srcs: Glob,
    #[serde(skip_serializing_if = "Set::is_empty")]
    pub(crate) tags: Set<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) target_compatible_with: Option<TargetCompatibleWith>,
    pub(crate) version: String,
}

#[derive(Debug)]
pub(crate) struct Data {
    pub(crate) glob: Glob,
    pub(crate) select: SelectSet<Label>,
}

impl Package {
    pub(crate) fn default_visibility_public(default_package_metadata: Set<Label>) -> Self {
        let mut default_visibility = Set::new();
        default_visibility.insert("//visibility:public".to_owned());
        Package {
            default_package_metadata,
            default_visibility,
        }
    }
}

impl Serialize for Alias {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Output looks like:
        //
        //     rule(
        //         name = "name",
        //         actual = "actual",
        //         tags = [
        //            "tag1",
        //            "tag2",
        //         ],
        //     )

        #[derive(Serialize)]
        struct AliasInner<'a> {
            pub(crate) name: &'a String,
            pub(crate) actual: &'a Label,
            pub(crate) tags: &'a Set<String>,
        }

        FunctionCall::new(
            &self.rule,
            AliasInner {
                name: &self.name,
                actual: &self.actual,
                tags: &self.tags,
            },
        )
        .serialize(serializer)
    }
}

pub(crate) fn serialize(starlark: &[Starlark]) -> Result<String, StarlarkError> {
    let mut content = String::new();
    for call in starlark {
        if !content.is_empty() {
            content.push('\n');
        }
        if let Starlark::Verbatim(comment) = call {
            content.push_str(comment);
        } else {
            content.push_str(&serde_starlark::to_string(call)?);
        }
    }
    Ok(content)
}
