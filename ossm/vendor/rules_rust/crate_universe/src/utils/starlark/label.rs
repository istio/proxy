use std::fmt::{self, Display};
use std::path::Path;
use std::str::FromStr;

use anyhow::{anyhow, bail, Context, Result};
use camino::Utf8Path;
use once_cell::sync::OnceCell;
use regex::Regex;
use serde::de::Visitor;
use serde::{Deserialize, Serialize, Serializer};

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub(crate) enum Label {
    Relative {
        target: String,
    },
    Absolute {
        repository: Repository,
        package: String,
        target: String,
    },
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub(crate) enum Repository {
    Canonical(String), // stringifies to `@@self.0` where `self.0` may be empty
    Explicit(String),  // stringifies to `@self.0` where `self.0` may be empty
    Local,             // stringifies to the empty string
}

impl Label {
    #[cfg(test)]
    pub(crate) fn is_absolute(&self) -> bool {
        match self {
            Label::Relative { .. } => false,
            Label::Absolute { .. } => true,
        }
    }

    #[cfg(test)]
    pub(crate) fn repository(&self) -> Option<&Repository> {
        match self {
            Label::Relative { .. } => None,
            Label::Absolute { repository, .. } => Some(repository),
        }
    }

    pub(crate) fn package(&self) -> Option<&str> {
        match self {
            Label::Relative { .. } => None,
            Label::Absolute { package, .. } => Some(package.as_str()),
        }
    }

    pub(crate) fn target(&self) -> &str {
        match self {
            Label::Relative { target } => target.as_str(),
            Label::Absolute { target, .. } => target.as_str(),
        }
    }
}

impl FromStr for Label {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        static RE: OnceCell<Regex> = OnceCell::new();
        let re = RE.get_or_try_init(|| {
            // TODO: Disallow `~` in repository names once support for Bazel 7.2 is dropped.
            Regex::new(r"^(@@?[\w\d\-_\.+~]*)?(//)?([\w\d\-_\./+]+)?(:([\+\w\d\-_\./]+))?$")
        });

        let cap = re?
            .captures(s)
            .with_context(|| format!("Failed to parse label from string: {s}"))?;

        let (repository, is_absolute) = match (cap.get(1), cap.get(2).is_some()) {
            (Some(repository), is_absolute) => match *repository.as_str().as_bytes() {
                [b'@', b'@', ..] => (
                    Some(Repository::Canonical(repository.as_str()[2..].to_owned())),
                    is_absolute,
                ),
                [b'@', ..] => (
                    Some(Repository::Explicit(repository.as_str()[1..].to_owned())),
                    is_absolute,
                ),
                _ => bail!("Invalid Label: {}", s),
            },
            (None, true) => (Some(Repository::Local), true),
            (None, false) => (None, false),
        };

        let package = cap.get(3).map(|package| package.as_str().to_owned());

        let target = cap.get(5).map(|target| target.as_str().to_owned());

        match repository {
            None => match (package, target) {
                // Relative
                (None, Some(target)) => Ok(Label::Relative { target }),

                // Relative (Implicit Target which regex identifies as Package)
                (Some(package), None) => Ok(Label::Relative { target: package }),

                // Invalid (Empty)
                (None, None) => bail!("Invalid Label: {}", s),

                // Invalid (Relative Package + Target)
                (Some(_), Some(_)) => bail!("Invalid Label: {}", s),
            },
            Some(repository) => match (is_absolute, package, target) {
                // Absolute (Full)
                (true, Some(package), Some(target)) => Ok(Label::Absolute {
                    repository,
                    package,
                    target,
                }),

                // Absolute (Repository)
                (_, None, None) => match &repository {
                    Repository::Canonical(target) | Repository::Explicit(target) => {
                        let target = match target.is_empty() {
                            false => target.clone(),
                            true => bail!("Invalid Label: {}", s),
                        };
                        Ok(Label::Absolute {
                            repository,
                            package: String::new(),
                            target,
                        })
                    }
                    Repository::Local => bail!("Invalid Label: {}", s),
                },

                // Absolute (Package)
                (true, Some(package), None) => {
                    let target = Utf8Path::new(&package)
                        .file_name()
                        .with_context(|| format!("Invalid Label: {}", s))?
                        .to_owned();
                    Ok(Label::Absolute {
                        repository,
                        package,
                        target,
                    })
                }

                // Absolute (Target)
                (true, None, Some(target)) => Ok(Label::Absolute {
                    repository,
                    package: String::new(),
                    target,
                }),

                // Invalid (Relative Repository + Package + Target)
                (false, Some(_), Some(_)) => bail!("Invalid Label: {}", s),

                // Invalid (Relative Repository + Package)
                (false, Some(_), None) => bail!("Invalid Label: {}", s),

                // Invalid (Relative Repository + Target)
                (false, None, Some(_)) => bail!("Invalid Label: {}", s),
            },
        }
    }
}

impl Display for Label {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Label::Relative { target } => write!(f, ":{}", target),
            Label::Absolute {
                repository,
                package,
                target,
            } => match repository {
                Repository::Canonical(repository) => {
                    write!(f, "@@{repository}//{package}:{target}")
                }
                Repository::Explicit(repository) => {
                    write!(f, "@{repository}//{package}:{target}")
                }
                Repository::Local => write!(f, "//{package}:{target}"),
            },
        }
    }
}

impl Label {
    /// Generates a label appropriate for the passed Path by walking the filesystem to identify its
    /// workspace and package.
    pub(crate) fn from_absolute_path(p: &Path) -> Result<Self, anyhow::Error> {
        let mut workspace_root = None;
        let mut package_root = None;
        for ancestor in p.ancestors().skip(1) {
            if package_root.is_none()
                && (ancestor.join("BUILD").exists() || ancestor.join("BUILD.bazel").exists())
            {
                package_root = Some(ancestor);
            }
            if workspace_root.is_none()
                && (ancestor.join("WORKSPACE").exists()
                    || ancestor.join("WORKSPACE.bazel").exists()
                    || ancestor.join("MODULE.bazel").exists())
            {
                workspace_root = Some(ancestor);
                break;
            }
        }
        match (workspace_root, package_root) {
            (Some(workspace_root), Some(package_root)) => {
                // These unwraps are safe by construction of the ancestors and prefix calls which set up these paths.
                let target = p.strip_prefix(package_root).unwrap();
                let workspace_relative = p.strip_prefix(workspace_root).unwrap();
                let mut package_path = workspace_relative.to_path_buf();
                for _ in target.components() {
                    package_path.pop();
                }

                let package = if package_path.components().count() > 0 {
                    path_to_label_part(&package_path)?
                } else {
                    String::new()
                };
                let target = path_to_label_part(target)?;

                Ok(Label::Absolute {
                    repository: Repository::Local,
                    package,
                    target,
                })
            }
            (Some(_workspace_root), None) => {
                bail!(
                    "Could not identify package for path {}. Maybe you need to add a BUILD.bazel file.",
                    p.display()
                );
            }
            _ => {
                bail!("Could not identify workspace for path {}", p.display());
            }
        }
    }
}

/// Converts a path to a forward-slash-delimited label-appropriate path string.
fn path_to_label_part(path: &Path) -> Result<String, anyhow::Error> {
    let components: Result<Vec<_>, _> = path
        .components()
        .map(|c| {
            c.as_os_str().to_str().ok_or_else(|| {
                anyhow!(
                    "Found non-UTF8 component turning path into label: {}",
                    path.display()
                )
            })
        })
        .collect();
    Ok(components?.join("/"))
}

impl Serialize for Label {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.repr())
    }
}

struct LabelVisitor;
impl Visitor<'_> for LabelVisitor {
    type Value = Label;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("Expected string value of `{name} {version}`.")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Label::from_str(v).map_err(E::custom)
    }
}

impl<'de> Deserialize<'de> for Label {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_str(LabelVisitor)
    }
}

impl Label {
    pub(crate) fn repr(&self) -> String {
        self.to_string()
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::fs::{create_dir_all, File};
    use tempfile::tempdir;

    #[test]
    fn relative() {
        let label = Label::from_str(":target").unwrap();
        assert_eq!(label.to_string(), ":target");
        assert!(!label.is_absolute());
        assert_eq!(label.repository(), None);
        assert_eq!(label.package(), None);
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn relative_implicit() {
        let label = Label::from_str("target").unwrap();
        assert_eq!(label.to_string(), ":target");
        assert!(!label.is_absolute());
        assert_eq!(label.repository(), None);
        assert_eq!(label.package(), None);
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn absolute_full() {
        let label = Label::from_str("@repo//package:target").unwrap();
        assert_eq!(label.to_string(), "@repo//package:target");
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Explicit(String::from("repo")))
        );
        assert_eq!(label.package(), Some("package"));
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn absolute_repository() {
        let label = Label::from_str("@repo").unwrap();
        assert_eq!(label.to_string(), "@repo//:repo");
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Explicit(String::from("repo")))
        );
        assert_eq!(label.package(), Some(""));
        assert_eq!(label.target(), "repo");
    }

    #[test]
    fn absolute_package() {
        let label = Label::from_str("//package").unwrap();
        assert_eq!(label.to_string(), "//package:package");
        assert!(label.is_absolute());
        assert_eq!(label.repository(), Some(&Repository::Local));
        assert_eq!(label.package(), Some("package"));
        assert_eq!(label.target(), "package");

        let label = Label::from_str("//package/subpackage").unwrap();
        assert_eq!(label.to_string(), "//package/subpackage:subpackage");
        assert!(label.is_absolute());
        assert_eq!(label.repository(), Some(&Repository::Local));
        assert_eq!(label.package(), Some("package/subpackage"));
        assert_eq!(label.target(), "subpackage");
    }

    #[test]
    fn absolute_target() {
        let label = Label::from_str("//:target").unwrap();
        assert_eq!(label.to_string(), "//:target");
        assert!(label.is_absolute());
        assert_eq!(label.repository(), Some(&Repository::Local));
        assert_eq!(label.package(), Some(""));
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn absolute_repository_package() {
        let label = Label::from_str("@repo//package").unwrap();
        assert_eq!(label.to_string(), "@repo//package:package");
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Explicit(String::from("repo")))
        );
        assert_eq!(label.package(), Some("package"));
        assert_eq!(label.target(), "package");
    }

    #[test]
    fn absolute_repository_target() {
        let label = Label::from_str("@repo//:target").unwrap();
        assert_eq!(label.to_string(), "@repo//:target");
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Explicit(String::from("repo")))
        );
        assert_eq!(label.package(), Some(""));
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn absolute_package_target() {
        let label = Label::from_str("//package:target").unwrap();
        assert_eq!(label.to_string(), "//package:target");
        assert!(label.is_absolute());
        assert_eq!(label.repository(), Some(&Repository::Local));
        assert_eq!(label.package(), Some("package"));
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn invalid_empty() {
        Label::from_str("").unwrap_err();
        Label::from_str("@").unwrap_err();
        Label::from_str("//").unwrap_err();
        Label::from_str(":").unwrap_err();
    }

    #[test]
    fn invalid_relative_repository_package_target() {
        Label::from_str("@repo/package:target").unwrap_err();
    }

    #[test]
    fn invalid_relative_repository_package() {
        Label::from_str("@repo/package").unwrap_err();
    }

    #[test]
    fn invalid_relative_repository_target() {
        Label::from_str("@repo:target").unwrap_err();
    }

    #[test]
    fn invalid_relative_package_target() {
        Label::from_str("package:target").unwrap_err();
    }

    #[test]
    fn full_label_bzlmod() {
        let label = Label::from_str("@@repo//package/sub_package:target").unwrap();
        assert_eq!(label.to_string(), "@@repo//package/sub_package:target");
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Canonical(String::from("repo")))
        );
        assert_eq!(label.package(), Some("package/sub_package"));
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn full_label_bzlmod_with_tilde() {
        let label = Label::from_str("@@repo~name//package/sub_package:target").unwrap();
        assert_eq!(label.to_string(), "@@repo~name//package/sub_package:target");
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Canonical(String::from("repo~name")))
        );
        assert_eq!(label.package(), Some("package/sub_package"));
        assert_eq!(label.target(), "target");
    }

    #[test]
    fn full_label_with_slash_after_colon() {
        let label = Label::from_str("@repo//package/sub_package:subdir/target").unwrap();
        assert_eq!(
            label.to_string(),
            "@repo//package/sub_package:subdir/target"
        );
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Explicit(String::from("repo")))
        );
        assert_eq!(label.package(), Some("package/sub_package"));
        assert_eq!(label.target(), "subdir/target");
    }

    #[test]
    fn label_contains_plus() {
        let label = Label::from_str("@repo//vendor/wasi-0.11.0+wasi-snapshot-preview1:BUILD.bazel")
            .unwrap();
        assert!(label.is_absolute());
        assert_eq!(
            label.repository(),
            Some(&Repository::Explicit(String::from("repo")))
        );
        assert_eq!(
            label.package(),
            Some("vendor/wasi-0.11.0+wasi-snapshot-preview1")
        );
        assert_eq!(label.target(), "BUILD.bazel");
    }

    #[test]
    fn invalid_double_colon() {
        Label::from_str("::target").unwrap_err();
    }

    #[test]
    fn invalid_triple_at() {
        Label::from_str("@@@repo//pkg:target").unwrap_err();
    }

    #[test]
    fn from_absolute_path_exists() {
        let dir = tempdir().unwrap();
        let workspace = dir.path().join("WORKSPACE.bazel");
        let build_file = dir.path().join("parent").join("child").join("BUILD.bazel");
        let subdir = dir.path().join("parent").join("child").join("grandchild");
        let actual_file = subdir.join("greatgrandchild");
        create_dir_all(subdir).unwrap();
        {
            File::create(workspace).unwrap();
            File::create(build_file).unwrap();
            File::create(&actual_file).unwrap();
        }
        let label = Label::from_absolute_path(&actual_file).unwrap();
        assert_eq!(
            label.to_string(),
            "//parent/child:grandchild/greatgrandchild"
        );
        assert!(label.is_absolute());
        assert_eq!(label.repository(), Some(&Repository::Local));
        assert_eq!(label.package(), Some("parent/child"));
        assert_eq!(label.target(), "grandchild/greatgrandchild");
    }

    #[test]
    fn from_absolute_path_no_workspace() {
        let dir = tempdir().unwrap();
        let build_file = dir.path().join("parent").join("child").join("BUILD.bazel");
        let subdir = dir.path().join("parent").join("child").join("grandchild");
        let actual_file = subdir.join("greatgrandchild");
        create_dir_all(subdir).unwrap();
        {
            File::create(build_file).unwrap();
            File::create(&actual_file).unwrap();
        }
        let err = Label::from_absolute_path(&actual_file)
            .unwrap_err()
            .to_string();
        assert!(err.contains("Could not identify workspace"));
        assert!(err.contains(format!("{}", actual_file.display()).as_str()));
    }

    #[test]
    fn from_absolute_path_no_build_file() {
        let dir = tempdir().unwrap();
        let workspace = dir.path().join("WORKSPACE.bazel");
        let subdir = dir.path().join("parent").join("child").join("grandchild");
        let actual_file = subdir.join("greatgrandchild");
        create_dir_all(subdir).unwrap();
        {
            File::create(workspace).unwrap();
            File::create(&actual_file).unwrap();
        }
        let err = Label::from_absolute_path(&actual_file)
            .unwrap_err()
            .to_string();
        assert!(err.contains("Could not identify package"));
        assert!(err.contains("Maybe you need to add a BUILD.bazel file"));
        assert!(err.contains(format!("{}", actual_file.display()).as_str()));
    }
}
