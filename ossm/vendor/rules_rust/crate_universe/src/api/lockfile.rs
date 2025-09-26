//! The lockfile::public module represents a reasonable stable API for inspecting the contents of a lockfile which others can code against.

use std::collections::BTreeSet;
use std::fs::File;
use std::io::BufReader;
use std::path::Path;

use anyhow::Result;
use serde::Deserialize;

pub use crate::config::CrateId;
use crate::context::crate_context::{CrateDependency, Rule};
use crate::context::{CommonAttributes, Context};
use crate::select::Select;

/// Parse a lockfile at a path on disk.
pub fn parse(path: &Path) -> Result<impl CargoBazelLockfile> {
    let reader = BufReader::new(File::open(path)?);
    let lockfile: CargoBazelLockfileImpl = serde_json::from_reader(reader)?;
    Ok(lockfile)
}

/// `CargoBazelLockfile` provides a view over `cargo-bazel`'s lockfile format.
///
/// This trait provides information about the third-party dependencies of a workspace.
/// While the lockfile's format doesn't provide any kind of compatibility guarantees over time,
/// this type offers an interface which is likely to be publicly supportable.
/// No formal compatibility guarantees are offered around this type - it may change at any time,
/// but the maintainers will attempt to keep it as stable they reasonably can.
pub trait CargoBazelLockfile {
    /// Get the members of the local workspace.
    /// These are typically not very interesting on their own, but can be used as roots for navigating what dependencies these crates have.
    fn workspace_members(&self) -> BTreeSet<CrateId>;

    /// Get information about a specific crate (which may be in the local workspace, or an external dependency).
    fn crate_info(&self, crate_id: &CrateId) -> Option<CrateInfo>;
}

#[derive(Deserialize)]
#[serde(transparent)]
struct CargoBazelLockfileImpl(Context);

impl CargoBazelLockfile for CargoBazelLockfileImpl {
    fn workspace_members(&self) -> BTreeSet<CrateId> {
        self.0.workspace_members.keys().cloned().collect()
    }

    fn crate_info(&self, crate_id: &CrateId) -> Option<CrateInfo> {
        let crate_context = self.0.crates.get(crate_id)?;
        Some(CrateInfo {
            name: crate_context.name.clone(),
            version: crate_context.version.clone(),
            library_target_name: crate_context.library_target_name.clone(),
            is_proc_macro: crate_context
                .targets
                .iter()
                .any(|t| matches!(t, Rule::ProcMacro(_))),
            common_attributes: crate_context.common_attrs.clone(),
        })
    }
}

/// Information about a crate (which may be in-workspace or a dependency).
#[derive(Deserialize, PartialEq, Eq, Debug)]
pub struct CrateInfo {
    name: String,
    version: semver::Version,
    library_target_name: Option<String>,
    is_proc_macro: bool,

    common_attributes: CommonAttributes,
}

impl CrateInfo {
    /// The name of the crate.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// The version of the crate.
    pub fn version(&self) -> &semver::Version {
        &self.version
    }

    /// The name of the crate's root library target. This is the target that a dependent
    /// would get if they were to depend on this crate.
    pub fn library_target_name(&self) -> Option<&str> {
        self.library_target_name.as_deref()
    }

    /// Whether the crate is a procedural macro.
    pub fn is_proc_macro(&self) -> bool {
        self.is_proc_macro
    }

    /// Dependencies required to compile the crate, without procedural macro dependencies.
    pub fn normal_deps(&self) -> Select<BTreeSet<CrateDependency>> {
        self.common_attributes.deps.clone()
    }

    /// Dependencies required to compile the tests for the crate, but not needed to compile the crate itself, without procedural macro dependencies.
    pub fn dev_deps(&self) -> Select<BTreeSet<CrateDependency>> {
        self.common_attributes.deps_dev.clone()
    }

    /// Procedural macro dependencies required to compile the crate.
    pub fn proc_macro_deps(&self) -> Select<BTreeSet<CrateDependency>> {
        self.common_attributes.proc_macro_deps.clone()
    }

    /// Procedural macro dependencies required to compile the tests for the crate, but not needed to compile the crate itself.
    pub fn proc_macro_dev_deps(&self) -> Select<BTreeSet<CrateDependency>> {
        self.common_attributes.proc_macro_deps_dev.clone()
    }
}

#[cfg(test)]
mod test {
    use super::{parse, CargoBazelLockfile};
    use crate::config::CrateId;
    use crate::context::crate_context::CrateDependency;
    use semver::Version;
    use std::collections::BTreeSet;

    #[test]
    fn exercise_public_lockfile_api() {
        let pkg_a = CrateId {
            name: String::from("pkg_a"),
            version: Version::new(0, 1, 0),
        };

        let want_workspace_member_names = {
            let mut set = BTreeSet::new();
            set.insert(pkg_a.clone());
            set.insert(CrateId {
                name: String::from("pkg_b"),
                version: Version::new(0, 1, 0),
            });
            set.insert(CrateId {
                name: String::from("pkg_c"),
                version: Version::new(0, 1, 0),
            });
            set
        };

        let runfiles = runfiles::Runfiles::create().unwrap();
        let path = runfiles::rlocation!(
            runfiles, "rules_rust/crate_universe/test_data/cargo_bazel_lockfile/multi_package-cargo-bazel-lock.json").unwrap();

        let parsed = parse(&path).unwrap();
        assert_eq!(parsed.workspace_members(), want_workspace_member_names);

        let got_pkg_a = parsed.crate_info(&pkg_a).unwrap();
        assert_eq!(got_pkg_a.name(), "pkg_a");
        assert_eq!(got_pkg_a.version(), &Version::new(0, 1, 0));
        assert_eq!(got_pkg_a.library_target_name(), Some("pkg_a"));
        assert!(!got_pkg_a.is_proc_macro());

        let serde_derive = CrateId {
            name: String::from("serde_derive"),
            version: Version::new(1, 0, 152),
        };
        let got_serde_derive = parsed.crate_info(&serde_derive).unwrap();
        assert_eq!(got_serde_derive.name(), "serde_derive");
        assert_eq!(got_serde_derive.version(), &Version::new(1, 0, 152));
        assert_eq!(got_serde_derive.library_target_name(), Some("serde_derive"));
        assert!(got_serde_derive.is_proc_macro);

        assert_eq!(
            got_pkg_a.normal_deps().values(),
            vec![
                CrateDependency {
                    id: CrateId {
                        name: String::from("anyhow"),
                        version: Version::new(1, 0, 69),
                    },
                    target: String::from("anyhow"),
                    alias: None,
                },
                CrateDependency {
                    id: CrateId {
                        name: String::from("reqwest"),
                        version: Version::new(0, 11, 14),
                    },
                    target: String::from("reqwest"),
                    alias: None,
                },
            ],
        );

        let async_process = CrateId {
            name: String::from("async-process"),
            version: Version::new(1, 6, 0),
        };
        let got_async_process = parsed.crate_info(&async_process).unwrap();
        let got_async_process_deps: BTreeSet<(Option<String>, String)> = got_async_process
            .normal_deps()
            .items()
            .into_iter()
            .map(|(config, dep)| (config, dep.id.name))
            .collect();
        assert_eq!(
            got_async_process_deps,
            vec![
                (None, "async-lock"),
                (None, "async-process"),
                (None, "cfg-if"),
                (None, "event-listener"),
                (None, "futures-lite"),
                (Some("cfg(unix)"), "async-io"),
                (Some("cfg(unix)"), "libc"),
                (Some("cfg(unix)"), "signal-hook"),
                (Some("cfg(windows)"), "blocking"),
                (Some("cfg(windows)"), "windows-sys"),
            ]
            .into_iter()
            .map(|(config, dep)| (config.map(String::from), String::from(dep)))
            .collect::<BTreeSet<_>>(),
        );
    }
}
