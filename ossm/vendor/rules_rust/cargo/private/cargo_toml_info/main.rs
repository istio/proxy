//! Command line tool invoked by Bazel to read metadata out of a `Cargo.toml` file.
//!
//! This tool should _not_ be used to determine dependencies, features, or generally any build
//! information for a crate, that should live in crate_universe. This tool is intended to read
//! non-build related Cargo metadata like lints, authors, or badges.

use cargo_toml::{Lint, LintLevel, Manifest};

use std::borrow::Cow;
use std::collections::BTreeMap;
use std::error::Error;
use std::fs::File;
use std::io::{LineWriter, Write};
use std::path::PathBuf;

pub fn main() -> Result<(), Box<dyn Error>> {
    let Args {
        manifest_toml,
        workspace_toml,
        command,
    } = Args::try_from(std::env::args())?;

    let manifest_contents = std::fs::read_to_string(&manifest_toml)?;
    let mut crate_manifest = Manifest::from_str(&manifest_contents)?;
    let mut workspace_manifest = None;

    // Optionally populate the manifest with info from the parent workspace, if one is provided.
    if let Some(workspace_path) = workspace_toml {
        let manifest = Manifest::from_path(&workspace_path)?;
        let workspace_details = Some((&manifest, workspace_path.as_path()));

        // TODO(parkmycar): Fix cargo_toml so we inherit lints from our workspace.
        //
        // See: <https://gitlab.com/lib.rs/cargo_toml/-/issues/35>
        crate_manifest.complete_from_path_and_workspace(&manifest_toml, workspace_details)?;
        workspace_manifest = Some(manifest);
    }

    match command {
        Command::Lints(args) => {
            generate_lints_info(&crate_manifest, workspace_manifest.as_ref(), args)?
        }
    }

    Ok(())
}

#[derive(Debug)]
struct LintsArgs {
    output_rustc_lints: PathBuf,
    output_clippy_lints: PathBuf,
    output_rustdoc_lints: PathBuf,
}

enum LintGroup {
    Rustc,
    Clippy,
    RustDoc,
}

impl LintGroup {
    pub fn key(&self) -> &'static str {
        match self {
            LintGroup::Rustc => "rust",
            LintGroup::Clippy => "clippy",
            LintGroup::RustDoc => "rustdoc",
        }
    }

    /// Format a lint `name` and `level` for this [`LintGroup`].
    pub fn format_cli_arg(&self, name: &str, level: LintLevel) -> String {
        let level = match level {
            LintLevel::Allow => "allow",
            LintLevel::Warn => "warn",
            LintLevel::Forbid => "forbid",
            LintLevel::Deny => "deny",
        };

        match self {
            LintGroup::Rustc => format!("--{level}={name}"),
            LintGroup::Clippy => format!("--{level}=clippy::{name}"),
            LintGroup::RustDoc => format!("--{level}=rustdoc::{name}"),
        }
    }
}

/// Returns the lints priority. It is needed in order to sort the lints according to their importance.
fn lint_priority(lint: &cargo_toml::Lint) -> i32 {
    match lint {
        cargo_toml::Lint::Detailed { level: _, priority } => priority.unwrap_or(0),
        cargo_toml::Lint::Simple(_) => 0,
    }
}

/// Formats the given lint set for the given group into CLI format.
///
/// This functions sorts the lints based on priority if given.
///
/// # Args
/// - `lints`: The lint to format, the value should come from the `lints.groups` field of the manifest.
/// - `group`: The lint group which needs formatting.
///
/// # Returns
///
/// An iterator of strings which can be passed to the linter as a CLI flag.
///
fn format_lint_set<'a>(
    lints: Option<&'a BTreeMap<String, BTreeMap<String, Lint>>>,
    group: &'a LintGroup,
) -> Option<impl Iterator<Item = String> + 'a> {
    let lints = lints?.get(group.key())?;
    let mut lints = Vec::from_iter(lints);
    lints.sort_by(|(_, a), (_, b)| {
        let a_priority = lint_priority(a);
        let b_priority = lint_priority(b);
        a_priority.cmp(&b_priority)
    });

    let formatted = lints.into_iter().map(|(name, lint)| {
        let level = match lint {
            cargo_toml::Lint::Detailed { level, priority: _ } => level,
            cargo_toml::Lint::Simple(level) => level,
        };
        group.format_cli_arg(name, *level)
    });

    Some(formatted)
}

/// Generates space separated <lint name> <lint level> files that get read back in by Bazel.
fn generate_lints_info(
    crate_manifest: &Manifest,
    workspace_manifest: Option<&Manifest>,
    args: LintsArgs,
) -> Result<(), Box<dyn Error>> {
    let LintsArgs {
        output_rustc_lints,
        output_clippy_lints,
        output_rustdoc_lints,
    } = args;

    let groups = [
        (LintGroup::Rustc, output_rustc_lints),
        (LintGroup::Clippy, output_clippy_lints),
        (LintGroup::RustDoc, output_rustdoc_lints),
    ];

    let lints = match &crate_manifest.lints {
        Some(lints) if lints.workspace => {
            let workspace = workspace_manifest
                .as_ref()
                .and_then(|manifest| manifest.workspace.as_ref())
                .ok_or({
                    "manifest inherits lints from the workspace, but no workspace manifest provided"
                })?;
            workspace.lints.as_ref()
        }
        Some(lints) => Some(&lints.groups),
        None => None,
    };

    for (group, path) in groups {
        let file = File::create(&path)?;
        let mut writer = LineWriter::new(file);

        if let Some(args) = format_lint_set(lints, &group) {
            for arg in args {
                writeln!(&mut writer, "{arg}")?;
            }
        };

        writer.flush()?;
    }

    Ok(())
}

#[derive(Debug)]
struct Args {
    manifest_toml: PathBuf,
    workspace_toml: Option<PathBuf>,
    command: Command,
}

impl TryFrom<std::env::Args> for Args {
    type Error = Cow<'static, str>;

    fn try_from(mut args: std::env::Args) -> Result<Self, Self::Error> {
        let _binary_path = args
            .next()
            .ok_or_else::<Cow<'static, str>, _>(|| "provided 0 arguments?".into())?;

        let mut args = args.peekable();

        // We get at least 'manifest-toml', and optionally a 'workspace-toml'.
        let manifest_raw_arg = args
            .next()
            .ok_or(Cow::Borrowed("expected at least one arg"))?;
        let manifest_toml =
            try_parse_named_arg(&manifest_raw_arg, "manifest_toml").map(PathBuf::from)?;
        let workspace_toml = args
            .peek()
            .and_then(|arg| try_parse_named_arg(arg, "workspace_toml").ok())
            .map(PathBuf::from);
        // If we got a workspace_toml arg make sure to consume it.
        if workspace_toml.is_some() {
            args.next();
        }

        // Use the remaining arguments to parse our command.
        let command = Command::try_from(RemainingArgs(args))?;

        Ok(Args {
            manifest_toml,
            workspace_toml,
            command,
        })
    }
}

/// Tries to parse the value from a named arg.
fn try_parse_named_arg<'a>(arg: &'a str, name: &str) -> Result<&'a str, String> {
    arg.strip_prefix(&format!("--{name}="))
        .ok_or_else(|| format!("expected --{name}=<value>, found '{arg}'"))
}

/// Arguments that are remaining after parsing the path to the `Cargo.toml`.
struct RemainingArgs(std::iter::Peekable<std::env::Args>);

#[derive(Debug)]
enum Command {
    /// Expects 4 filesystem paths in this order:
    ///
    /// 1. output for rustc lints
    /// 2. output for clippy lints
    /// 3. output for rustdoc lints
    ///
    Lints(LintsArgs),
}

impl TryFrom<RemainingArgs> for Command {
    type Error = Cow<'static, str>;

    fn try_from(args: RemainingArgs) -> Result<Self, Self::Error> {
        let RemainingArgs(args) = args;
        let mut args = args.peekable();

        let action = args
            .next()
            .ok_or_else::<Cow<'static, str>, _>(|| "expected an action".into())?;

        match action.to_lowercase().as_str() {
            "lints" => {
                let output_rustc_lints = args
                    .next()
                    .map(PathBuf::from)
                    .ok_or(Cow::Borrowed("expected output path for rustc lints"))?;
                let output_clippy_lints = args
                    .next()
                    .map(PathBuf::from)
                    .ok_or(Cow::Borrowed("expected output path for clippy lints"))?;
                let output_rustdoc_lints = args
                    .next()
                    .map(PathBuf::from)
                    .ok_or(Cow::Borrowed("expected output path for rustdoc lints"))?;

                if args.peek().is_some() {
                    let remaining: Vec<String> = args.collect();
                    let msg = format!("expected end of arguments, found: {remaining:?}");
                    return Err(Cow::Owned(msg));
                }

                Ok(Command::Lints(LintsArgs {
                    output_rustc_lints,
                    output_clippy_lints,
                    output_rustdoc_lints,
                }))
            }
            other => Err(format!("unknown action: {other}").into()),
        }
    }
}

#[cfg(test)]
mod test {
    use cargo_toml::{Lint, LintLevel};
    use std::collections::BTreeMap;

    /// Tests priority handling of different lints
    #[test]
    fn format_lint_set() {
        assert!(super::format_lint_set(None, &super::LintGroup::Rustc).is_none());

        let lints_map: BTreeMap<String, BTreeMap<String, Lint>> = BTreeMap::from_iter([(
            "clippy".into(),
            BTreeMap::from_iter([
                ("rustc_allow".into(), Lint::Simple(LintLevel::Allow)),
                ("rustc_forbid".into(), Lint::Simple(LintLevel::Forbid)),
                ("clippy_warn".into(), Lint::Simple(LintLevel::Warn)),
                (
                    "rustdoc_forbid".into(),
                    Lint::Detailed {
                        level: LintLevel::Forbid,
                        priority: Some(20),
                    },
                ),
                ("rustc_warn".into(), Lint::Simple(LintLevel::Warn)),
                (
                    "rustc_deny".into(),
                    Lint::Detailed {
                        level: LintLevel::Deny,
                        priority: Some(-2),
                    },
                ),
                ("rustdoc_deny".into(), Lint::Simple(LintLevel::Deny)),
                ("rustdoc_allow".into(), Lint::Simple(LintLevel::Allow)),
                (
                    "clippy_forbid".into(),
                    Lint::Detailed {
                        level: LintLevel::Forbid,
                        priority: Some(-1),
                    },
                ),
                (
                    "clippy_deny".into(),
                    Lint::Detailed {
                        level: LintLevel::Deny,
                        priority: Some(-1),
                    },
                ),
                (
                    "rustc_deny_without_priority".into(),
                    Lint::Detailed {
                        level: LintLevel::Deny,
                        priority: None,
                    },
                ),
                (
                    "rustc_deny_without_priority2".into(),
                    Lint::Detailed {
                        level: LintLevel::Deny,
                        priority: Some(0),
                    },
                ),
            ]),
        )]);

        assert!(super::format_lint_set(Some(&lints_map), &super::LintGroup::Rustc).is_none());

        let lints: Vec<String> =
            super::format_lint_set(Some(&lints_map), &super::LintGroup::Clippy)
                .unwrap()
                .collect();
        assert_eq!(
            lints,
            [
                "--deny=clippy::rustc_deny",
                "--deny=clippy::clippy_deny",
                "--forbid=clippy::clippy_forbid",
                "--warn=clippy::clippy_warn",
                "--allow=clippy::rustc_allow",
                "--deny=clippy::rustc_deny_without_priority",
                "--deny=clippy::rustc_deny_without_priority2",
                "--forbid=clippy::rustc_forbid",
                "--warn=clippy::rustc_warn",
                "--allow=clippy::rustdoc_allow",
                "--deny=clippy::rustdoc_deny",
                "--forbid=clippy::rustdoc_forbid"
            ]
        );
    }

    #[test]
    fn lint_priority() {
        // Test different lint priority scenarios
        let simple_lint = Lint::Simple(LintLevel::Allow);
        assert_eq!(super::lint_priority(&simple_lint), 0);

        let detailed_no_priority = Lint::Detailed {
            level: LintLevel::Allow,
            priority: None,
        };
        assert_eq!(super::lint_priority(&detailed_no_priority), 0);

        let detailed_with_priority = Lint::Detailed {
            level: LintLevel::Allow,
            priority: Some(5),
        };
        assert_eq!(super::lint_priority(&detailed_with_priority), 5);

        let detailed_negative_priority = Lint::Detailed {
            level: LintLevel::Allow,
            priority: Some(-3),
        };
        assert_eq!(super::lint_priority(&detailed_negative_priority), -3);
    }
}
