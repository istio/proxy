use std::collections::{BTreeMap, BTreeSet};
use std::path::Path;

use anyhow::{anyhow, bail, Context, Result};
use camino::{Utf8Path, Utf8PathBuf};
use cargo_toml::Manifest;

/// A description of Cargo.toml files and how they are related in workspaces.
/// All `Utf8PathBuf` values are paths of Cargo.toml files.
#[derive(Debug, PartialEq)]
pub(crate) struct DiscoveredWorkspaces {
    workspaces_to_members: BTreeMap<Utf8PathBuf, BTreeSet<Utf8PathBuf>>,
    non_workspaces: BTreeSet<Utf8PathBuf>,
}

impl DiscoveredWorkspaces {
    pub(crate) fn workspaces(&self) -> BTreeSet<Utf8PathBuf> {
        self.workspaces_to_members.keys().cloned().collect()
    }

    pub(crate) fn all_workspaces_and_members(&self) -> BTreeSet<Utf8PathBuf> {
        self.workspaces_to_members
            .keys()
            .chain(self.workspaces_to_members.values().flatten())
            .cloned()
            .collect()
    }
}

pub(crate) fn discover_workspaces(
    cargo_toml_paths: BTreeSet<Utf8PathBuf>,
    known_manifests: &BTreeMap<Utf8PathBuf, Manifest>,
    bazel_workspace_root: &Path,
) -> Result<DiscoveredWorkspaces> {
    let mut manifest_cache = ManifestCache {
        cache: BTreeMap::new(),
        known_manifests,
    };
    discover_workspaces_with_cache(cargo_toml_paths, bazel_workspace_root, &mut manifest_cache)
}

fn discover_workspaces_with_cache(
    cargo_toml_paths: BTreeSet<Utf8PathBuf>,
    bazel_workspace_root: &Path,
    manifest_cache: &mut ManifestCache,
) -> Result<DiscoveredWorkspaces> {
    let mut discovered_workspaces = DiscoveredWorkspaces {
        workspaces_to_members: BTreeMap::new(),
        non_workspaces: BTreeSet::new(),
    };

    // First pass: Discover workspace parents.
    for cargo_toml_path in cargo_toml_paths {
        if let Some(workspace_parent) = discover_workspace_parent(&cargo_toml_path, manifest_cache)
        {
            discovered_workspaces
                .workspaces_to_members
                .insert(workspace_parent, BTreeSet::new());
        } else {
            discovered_workspaces.non_workspaces.insert(cargo_toml_path);
        }
    }

    // Second pass: Find all child manifests.
    for workspace_path in discovered_workspaces
        .workspaces_to_members
        .keys()
        .cloned()
        .collect::<BTreeSet<_>>()
    {
        let workspace_manifest = manifest_cache.get(&workspace_path).unwrap();
        'per_child: for entry in walkdir::WalkDir::new(workspace_path.parent().unwrap())
            .follow_links(true)
            .follow_root_links(true)
            .into_iter()
            // Avoid traversing the bazel-$workspace symlink which mirrors the whole source root.
            // This is not super correct - technically the symlinks can be renamed,
            // and technically people can create symlinks they care about which match this pattern to.
            // But it's Good Enough.
            .filter_entry(|e| {
                if !e.path_is_symlink() {
                    return true;
                }
                if e.path().parent().unwrap() != bazel_workspace_root {
                    return true;
                }
                if let Some(file_name) = e.file_name().to_str() {
                    if file_name.starts_with("bazel-") || file_name.starts_with(".bazel") {
                        return false;
                    }
                }
                true
            })
        {
            let entry = match entry {
                Ok(entry) => entry,
                Err(err) => {
                    if let Some(io_err) = err.io_error() {
                        if let Some(path) = err.path() {
                            if let Ok(symlink_metadata) = std::fs::symlink_metadata(path) {
                                if symlink_metadata.is_symlink()
                                    && io_err.kind() == std::io::ErrorKind::NotFound
                                {
                                    // Ignore dangling symlinks
                                    continue;
                                }
                            }
                        }
                    }
                    return Err(err)
                        .context("Failed to walk filesystem finding workspace Cargo.toml files");
                }
            };

            if entry.file_name() != "Cargo.toml" {
                continue;
            }

            let child_path = Utf8Path::from_path(entry.path())
                .ok_or_else(|| anyhow!("Failed to parse {:?} as UTF-8", entry.path()))?
                .to_path_buf();
            if child_path == workspace_path {
                continue;
            }

            let manifest = manifest_cache
                .get(&child_path)
                .ok_or_else(|| anyhow!("Failed to read manifest at {}", child_path))?;

            let mut actual_workspace_path = workspace_path.clone();
            if let Some(package) = manifest.package {
                if let Some(explicit_workspace_path) = package.workspace {
                    actual_workspace_path =
                        child_path.parent().unwrap().join(explicit_workspace_path);
                }
            }
            if !discovered_workspaces
                .workspaces_to_members
                .contains_key(&actual_workspace_path)
            {
                bail!("Found manifest at {} which is a member of the workspace at {} which isn't included in the crates_universe", child_path, actual_workspace_path);
            }

            for exclude in &workspace_manifest.workspace.as_ref().unwrap().exclude {
                let exclude_path = workspace_path.parent().unwrap().join(exclude);
                if exclude_path == child_path.parent().unwrap() {
                    discovered_workspaces.non_workspaces.insert(child_path);
                    continue 'per_child;
                }
            }

            discovered_workspaces
                .workspaces_to_members
                .get_mut(&actual_workspace_path)
                .unwrap()
                .insert(child_path.clone());
        }
    }

    Ok(discovered_workspaces)
}

fn discover_workspace_parent(
    cargo_toml_path: &Utf8PathBuf,
    manifest_cache: &mut ManifestCache,
) -> Option<Utf8PathBuf> {
    for parent_dir in cargo_toml_path.ancestors().skip(1) {
        let maybe_cargo_toml_path = parent_dir.join("Cargo.toml");
        let maybe_manifest = manifest_cache.get(&maybe_cargo_toml_path);
        if let Some(manifest) = maybe_manifest {
            if manifest.workspace.is_some() {
                return Some(maybe_cargo_toml_path);
            }
        }
    }
    None
}

struct ManifestCache<'a> {
    cache: BTreeMap<Utf8PathBuf, Option<Manifest>>,
    known_manifests: &'a BTreeMap<Utf8PathBuf, Manifest>,
}

impl ManifestCache<'_> {
    fn get(&mut self, path: &Utf8PathBuf) -> Option<Manifest> {
        if let Some(manifest) = self.known_manifests.get(path) {
            return Some(manifest.clone());
        }
        if let Some(maybe_manifest) = self.cache.get(path) {
            return maybe_manifest.clone();
        }
        let maybe_manifest = if let Ok(manifest) = Manifest::from_path(path) {
            Some(manifest)
        } else {
            None
        };
        self.cache.insert(path.clone(), maybe_manifest.clone());
        maybe_manifest
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::path::{Path, PathBuf};
    use std::sync::Mutex;

    // Both of these tests try to create the same symlink, so they can't run in parallel.
    static FILESYSTEM_GUARD: Mutex<()> = Mutex::new(());

    #[test]
    fn test_discover() {
        let _guard = FILESYSTEM_GUARD.lock().unwrap();
        let r = runfiles::Runfiles::create().unwrap();
        let root_dir =
            runfiles::rlocation!(r, "rules_rust/crate_universe/test_data/workspace_examples")
                .unwrap();
        let root_dir = Utf8PathBuf::from_path_buf(root_dir).unwrap();

        let _manifest_symlink = DeleteOnDropDirSymlink::symlink(
            Path::new("..").join("symlinked"),
            root_dir.join("ws1").join("bazel-ws1").into_std_path_buf(),
        )
        .unwrap();

        let mut expected = ws1_discovered_workspaces(&root_dir);

        // This isn't at the bazel repo root level, so gets included.
        expected
            .workspaces_to_members
            .get_mut(&root_dir.join("ws1").join("Cargo.toml"))
            .unwrap()
            .insert(root_dir.join("ws1").join("bazel-ws1").join("Cargo.toml"));

        expected.workspaces_to_members.insert(
            root_dir.join("ws2").join("Cargo.toml"),
            BTreeSet::from([
                root_dir.join("ws2").join("ws2c1").join("Cargo.toml"),
                root_dir
                    .join("ws2")
                    .join("ws2excluded")
                    .join("ws2included")
                    .join("Cargo.toml"),
            ]),
        );

        expected.non_workspaces.extend([
            root_dir.join("non-ws").join("Cargo.toml"),
            root_dir.join("ws2").join("ws2excluded").join("Cargo.toml"),
            root_dir
                .join("ws2")
                .join("ws2excluded")
                .join("ws2excluded2")
                .join("Cargo.toml"),
        ]);

        let actual = discover_workspaces(
            vec![
                root_dir.join("ws1/ws1c1/Cargo.toml"),
                root_dir.join("ws2/Cargo.toml"),
                root_dir.join("non-ws/Cargo.toml"),
            ]
            .into_iter()
            .collect(),
            &BTreeMap::new(),
            root_dir.as_std_path(),
        )
        .unwrap();

        assert_eq!(expected, actual);
    }

    #[test]
    fn test_ignore_bazel_root_symlink() {
        let _guard = FILESYSTEM_GUARD.lock().unwrap();
        let r = runfiles::Runfiles::create().unwrap();
        let root_dir =
            runfiles::rlocation!(r, "rules_rust/crate_universe/test_data/workspace_examples")
                .unwrap();
        let root_dir = Utf8PathBuf::from_path_buf(root_dir).unwrap();

        let _symlink1 = DeleteOnDropDirSymlink::symlink(
            Path::new("..").join("symlinked"),
            root_dir.join("ws1").join("bazel-ws1").into_std_path_buf(),
        )
        .unwrap();

        let _symlink2 = DeleteOnDropDirSymlink::symlink(
            Path::new("..").join("symlinked"),
            root_dir.join("ws1").join(".bazel").into_std_path_buf(),
        )
        .unwrap();

        let expected = ws1_discovered_workspaces(&root_dir);

        let actual = discover_workspaces(
            vec![root_dir.join("ws1/ws1c1/Cargo.toml")]
                .into_iter()
                .collect(),
            &BTreeMap::new(),
            root_dir.join("ws1").as_std_path(),
        )
        .unwrap();

        assert_eq!(expected, actual);
    }

    #[test]
    fn test_discover_ignores_dangling_symlinks() {
        let _guard = FILESYSTEM_GUARD.lock().unwrap();
        let r = runfiles::Runfiles::create().unwrap();
        let root_dir =
            runfiles::rlocation!(r, "rules_rust/crate_universe/test_data/workspace_examples")
                .unwrap();
        let root_dir = Utf8PathBuf::from_path_buf(root_dir).unwrap();

        let _dangling_symlink = DeleteOnDropDirSymlink::symlink(
            non_existing_path(),
            root_dir.join("ws1").join("dangling").into_std_path_buf(),
        )
        .unwrap();

        let expected = ws1_discovered_workspaces(&root_dir);

        let actual = discover_workspaces(
            vec![root_dir.join("ws1/ws1c1/Cargo.toml")]
                .into_iter()
                .collect(),
            &BTreeMap::new(),
            root_dir.as_std_path(),
        )
        .unwrap();

        assert_eq!(expected, actual);
    }

    fn ws1_discovered_workspaces(root_dir: &Utf8Path) -> DiscoveredWorkspaces {
        let mut workspaces_to_members = BTreeMap::new();
        workspaces_to_members.insert(
            root_dir.join("ws1").join("Cargo.toml"),
            BTreeSet::from([
                root_dir.join("ws1").join("ws1c1").join("Cargo.toml"),
                root_dir
                    .join("ws1")
                    .join("ws1c1")
                    .join("ws1c1c1")
                    .join("Cargo.toml"),
                root_dir.join("ws1").join("ws1c2").join("Cargo.toml"),
            ]),
        );
        let non_workspaces = BTreeSet::new();

        DiscoveredWorkspaces {
            workspaces_to_members,
            non_workspaces,
        }
    }

    struct DeleteOnDropDirSymlink(PathBuf);

    impl DeleteOnDropDirSymlink {
        #[cfg(unix)]
        fn symlink<P: AsRef<Path>>(original: P, link: PathBuf) -> std::io::Result<Self> {
            std::os::unix::fs::symlink(original, &link)?;
            Ok(Self(link))
        }

        #[cfg(windows)]
        fn symlink<P: AsRef<Path>>(original: P, link: PathBuf) -> std::io::Result<Self> {
            std::os::windows::fs::symlink_dir(original, &link)?;
            Ok(Self(link))
        }
    }

    impl Drop for DeleteOnDropDirSymlink {
        #[cfg(unix)]
        fn drop(&mut self) {
            std::fs::remove_file(&self.0).expect("Failed to delete symlink");
        }
        #[cfg(windows)]
        fn drop(&mut self) {
            std::fs::remove_dir(&self.0).expect("Failed to delete symlink");
        }
    }

    #[cfg(unix)]
    fn non_existing_path() -> PathBuf {
        PathBuf::from("/doesnotexist")
    }

    #[cfg(windows)]
    fn non_existing_path() -> PathBuf {
        PathBuf::from("Z:\\doesnotexist")
    }
}
