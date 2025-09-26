//! Bazel label parsing library.
//!
//! USAGE: `label::analyze("//foo/bar:baz")
mod label_error;
use label_error::LabelError;

/// Parse and analyze given str.
///
/// TODO: validate . and .. in target name
/// TODO: validate used characters in target name
pub fn analyze(input: &'_ str) -> Result<Label<'_>> {
    Label::analyze(input)
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub enum Repository<'s> {
    /// A `@@` prefixed name that is unique within a workspace. E.g. `@@rules_rust+0.1.2+toolchains~local_rustc`
    Canonical(&'s str), // stringifies to `@@self.0` where `self.0` may be empty
    /// A `@` (single) prefixed name. E.g. `@rules_rust`.
    Apparent(&'s str),
}

impl<'s> Repository<'s> {
    pub fn repo_name(&self) -> &'s str {
        match self {
            Repository::Canonical(name) => &name[2..],
            Repository::Apparent(name) => &name[1..],
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum Label<'s> {
    Relative {
        target_name: &'s str,
    },
    Absolute {
        repository: Option<Repository<'s>>,
        package_name: &'s str,
        target_name: &'s str,
    },
}

type Result<T, E = LabelError> = core::result::Result<T, E>;

impl<'s> Label<'s> {
    /// Parse and analyze given str.
    pub fn analyze(input: &'s str) -> Result<Label<'s>> {
        let label = input;

        if label.is_empty() {
            return Err(LabelError(err(
                label,
                "Empty string cannot be parsed into a label.",
            )));
        }

        if label.starts_with(':') {
            return match consume_name(input, label)? {
                None => Err(LabelError(err(
                    label,
                    "Relative packages must have a name.",
                ))),
                Some(name) => Ok(Label::Relative { target_name: name }),
            };
        }

        let (input, repository) = consume_repository_name(input, label)?;

        // Shorthand labels such as `@repo` are expanded to `@repo//:repo`.
        if input.is_empty() {
            if let Some(ref repo) = repository {
                let target_name = repo.repo_name();
                if target_name.is_empty() {
                    return Err(LabelError(err(
                        label,
                        "invalid target name: empty target name",
                    )));
                } else {
                    return Ok(Label::Absolute {
                        repository,
                        package_name: "",
                        target_name,
                    });
                };
            }
        }
        let (input, package_name) = consume_package_name(input, label)?;
        let name = consume_name(input, label)?;
        let name = match (package_name, name) {
            (None, None) => {
                return Err(LabelError(err(
                    label,
                    "labels must have a package and/or a name.",
                )))
            }
            (Some(package_name), None) => name_from_package(package_name),
            (_, Some(name)) => name,
        };

        Ok(Label::Absolute {
            repository,
            package_name: package_name.unwrap_or_default(),
            target_name: name,
        })
    }

    pub fn is_relative(&self) -> bool {
        match self {
            Label::Absolute { .. } => false,
            Label::Relative { .. } => true,
        }
    }

    pub fn repo(&self) -> Option<&Repository<'s>> {
        match self {
            Label::Absolute { repository, .. } => repository.as_ref(),
            Label::Relative { .. } => None,
        }
    }

    pub fn repo_name(&self) -> Option<&'s str> {
        match self {
            Label::Absolute { repository, .. } => repository.as_ref().map(|repo| repo.repo_name()),
            Label::Relative { .. } => None,
        }
    }

    pub fn package(&self) -> Option<&'s str> {
        match self {
            Label::Relative { .. } => None,
            Label::Absolute { package_name, .. } => Some(*package_name),
        }
    }

    pub fn name(&self) -> &'s str {
        match self {
            Label::Relative { target_name } => target_name,
            Label::Absolute { target_name, .. } => target_name,
        }
    }
}

fn err<'s>(label: &'s str, msg: &'s str) -> String {
    let mut err_msg = label.to_string();
    err_msg.push_str(" must be a legal label; ");
    err_msg.push_str(msg);
    err_msg
}

fn consume_repository_name<'s>(
    input: &'s str,
    label: &'s str,
) -> Result<(&'s str, Option<Repository<'s>>)> {
    let at_signs = {
        let mut count = 0;
        for char in input.chars() {
            if char == '@' {
                count += 1;
            } else {
                break;
            }
        }
        count
    };
    if at_signs == 0 {
        return Ok((input, None));
    }
    if at_signs > 2 {
        return Err(LabelError(err(label, "Unexpected number of leading `@`.")));
    }

    let slash_pos = input.find("//").unwrap_or(input.len());
    let repository_name = &input[at_signs..slash_pos];

    if !repository_name.is_empty() {
        if !repository_name
            .chars()
            .next()
            .unwrap()
            .is_ascii_alphabetic()
        {
            return Err(LabelError(err(
                label,
                "workspace names must start with a letter.",
            )));
        }
        if !repository_name
            .chars()
            // TODO: Disallow `~` in repository names once support for Bazel 7.2 is dropped.
            .all(|c| {
                c.is_ascii_alphanumeric()
                    || c == '-'
                    || c == '_'
                    || c == '.'
                    || c == '~'
                    || c == '+'
            })
        {
            return Err(LabelError(err(
                label,
                "workspace names \
                may contain only A-Z, a-z, 0-9, '-', '_', '.', '+', and '~'.",
            )));
        }
    }

    let repository = if at_signs == 1 {
        Repository::Apparent(&input[0..slash_pos])
    } else if at_signs == 2 {
        if repository_name.is_empty() {
            return Err(LabelError(err(
                label,
                "main repository labels are only represented by a single `@`.",
            )));
        }
        Repository::Canonical(&input[0..slash_pos])
    } else {
        return Err(LabelError(err(label, "Unexpected number of leading `@`.")));
    };

    Ok((&input[slash_pos..], Some(repository)))
}

fn consume_package_name<'s>(input: &'s str, label: &'s str) -> Result<(&'s str, Option<&'s str>)> {
    let is_absolute = match input.rfind("//") {
        None => false,
        Some(0) => true,
        Some(_) => {
            return Err(LabelError(err(
                label,
                "'//' cannot appear in the middle of the label.",
            )));
        }
    };

    let (package_name, rest) = match (is_absolute, input.find(':')) {
        (false, colon_pos) if (colon_pos != Some(0)) => {
            return Err(LabelError(err(
                label,
                "relative packages are not permitted.",
            )));
        }
        (_, colon_pos) => {
            let (input, colon_pos) = if is_absolute {
                (&input[2..], colon_pos.map(|cp| cp - 2))
            } else {
                (input, colon_pos)
            };
            match colon_pos {
                Some(colon_pos) => (&input[0..colon_pos], &input[colon_pos..]),
                None => (input, ""),
            }
        }
    };

    if package_name.is_empty() {
        return Ok((rest, None));
    }

    if !package_name.chars().all(|c| {
        c.is_ascii_alphanumeric()
            || c == '/'
            || c == '-'
            || c == '.'
            || c == ' '
            || c == '$'
            || c == '('
            || c == ')'
            || c == '_'
            || c == '+'
    }) {
        return Err(LabelError(err(
            label,
            "package names may contain only A-Z, \
        a-z, 0-9, '/', '-', '.', ' ', '$', '(', ')', '_', and '+'.",
        )));
    }
    if package_name.ends_with('/') {
        return Err(LabelError(err(
            label,
            "package names may not end with '/'.",
        )));
    }

    if rest.is_empty() && is_absolute {
        // This label doesn't contain the target name, we have to use
        // last segment of the package name as target name.
        return Ok((
            match package_name.rfind('/') {
                Some(pos) => &package_name[pos..],
                None => package_name,
            },
            Some(package_name),
        ));
    }

    Ok((rest, Some(package_name)))
}

fn consume_name<'s>(input: &'s str, label: &'s str) -> Result<Option<&'s str>> {
    if input.is_empty() {
        return Ok(None);
    }
    if input == ":" {
        return Err(LabelError(err(label, "empty target name.")));
    }
    let name = if let Some(stripped) = input.strip_prefix(':') {
        stripped
    } else if let Some(stripped) = input.strip_prefix("//") {
        stripped
    } else {
        input.strip_prefix('/').unwrap_or(input)
    };

    if name.starts_with('/') {
        return Err(LabelError(err(
            label,
            "target names may not start with '/'.",
        )));
    }
    if name.starts_with(':') {
        return Err(LabelError(err(
            label,
            "target names may not contain with ':'.",
        )));
    }
    Ok(Some(name))
}

fn name_from_package(package_name: &str) -> &str {
    package_name
        .rsplit_once('/')
        .map(|tup| tup.1)
        .unwrap_or(package_name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_repository_name_parsing() -> Result<()> {
        assert_eq!(analyze("@repo//:foo")?.repo_name(), Some("repo"));
        assert_eq!(analyze("@repo+name//:foo")?.repo_name(), Some("repo+name"));
        assert_eq!(analyze("@@repo//:foo")?.repo_name(), Some("repo"));
        assert_eq!(analyze("@//:foo")?.repo_name(), Some(""));
        assert_eq!(analyze("//:foo")?.repo_name(), None);
        assert_eq!(analyze(":foo")?.repo_name(), None);

        assert_eq!(analyze("@repo//foo/bar")?.repo_name(), Some("repo"));
        assert_eq!(analyze("@@repo//foo/bar")?.repo_name(), Some("repo"));
        assert_eq!(
            analyze("@@repo+name//foo/bar")?.repo_name(),
            Some("repo+name")
        );
        assert_eq!(analyze("@//foo/bar")?.repo_name(), Some(""));
        assert_eq!(analyze("//foo/bar")?.repo_name(), None);
        assert_eq!(
            analyze("foo/bar"),
            Err(LabelError(
                "foo/bar must be a legal label; relative packages are not permitted.".to_string()
            ))
        );

        assert_eq!(analyze("@repo//foo")?.repo_name(), Some("repo"));
        assert_eq!(analyze("@@repo//foo")?.repo_name(), Some("repo"));
        assert_eq!(analyze("@//foo")?.repo_name(), Some(""));
        assert_eq!(analyze("//foo")?.repo_name(), None);
        assert_eq!(
            analyze("foo"),
            Err(LabelError(
                "foo must be a legal label; relative packages are not permitted.".to_string()
            ))
        );

        assert_eq!(
            analyze("@@@repo//foo"),
            Err(LabelError(
                "@@@repo//foo must be a legal label; Unexpected number of leading `@`.".to_owned()
            ))
        );

        assert_eq!(
            analyze("@@@//foo:bar"),
            Err(LabelError(
                "@@@//foo:bar must be a legal label; Unexpected number of leading `@`.".to_owned()
            ))
        );

        assert_eq!(
            analyze("@foo:bar"),
            Err(LabelError(
                "@foo:bar must be a legal label; workspace names may contain only A-Z, a-z, 0-9, '-', '_', '.', '+', and '~'.".to_string()
            ))
        );

        assert_eq!(
            analyze("@AZab0123456789_-.//:foo")?.repo_name(),
            Some("AZab0123456789_-.")
        );
        assert_eq!(
            analyze("@42//:baz"),
            Err(LabelError(
                "@42//:baz must be a legal label; workspace names must \
            start with a letter."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("@foo#//:baz"),
            Err(LabelError(
                "@foo#//:baz must be a legal label; workspace names \
            may contain only A-Z, a-z, 0-9, '-', '_', '.', '+', and '~'."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("@@//foo/bar"),
            Err(LabelError(
                "@@//foo/bar must be a legal label; main repository labels are only represented by a single `@`."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("@@//:foo"),
            Err(LabelError(
                "@@//:foo must be a legal label; main repository labels are only represented by a single `@`."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("@@//foo"),
            Err(LabelError(
                "@@//foo must be a legal label; main repository labels are only represented by a single `@`."
                    .to_string()
            ))
        );

        assert_eq!(
            analyze("@@"),
            Err(LabelError(
                "@@ must be a legal label; main repository labels are only represented by a single `@`.".to_string()
            )),
        );

        Ok(())
    }

    #[test]
    fn test_package_name_parsing() -> Result<()> {
        assert_eq!(analyze("//:baz/qux")?.package(), Some(""));
        assert_eq!(analyze(":baz/qux")?.package(), None);

        assert_eq!(analyze("//foo:baz/qux")?.package(), Some("foo"));
        assert_eq!(analyze("//foo/bar:baz/qux")?.package(), Some("foo/bar"));
        assert_eq!(
            analyze("foo:baz/qux"),
            Err(LabelError(
                "foo:baz/qux must be a legal label; relative packages are not permitted."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("foo/bar:baz/qux"),
            Err(LabelError(
                "foo/bar:baz/qux must be a legal label; relative packages are not permitted."
                    .to_string()
            ))
        );

        assert_eq!(analyze("//foo")?.package(), Some("foo"));

        assert_eq!(
            analyze("foo//bar"),
            Err(LabelError(
                "foo//bar must be a legal label; '//' cannot appear in the middle of the label."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("//foo//bar"),
            Err(LabelError(
                "//foo//bar must be a legal label; '//' cannot appear in the middle of the label."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("foo//bar:baz"),
            Err(LabelError(
                "foo//bar:baz must be a legal label; '//' cannot appear in the middle of the label."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("//foo//bar:baz"),
            Err(LabelError(
                "//foo//bar:baz must be a legal label; '//' cannot appear in the middle of the label."
                    .to_string()
            ))
        );

        assert_eq!(
            analyze("//azAZ09/-. $()_:baz")?.package(),
            Some("azAZ09/-. $()_")
        );
        assert_eq!(
            analyze("//bar#:baz"),
            Err(LabelError(
                "//bar#:baz must be a legal label; package names may contain only A-Z, \
                a-z, 0-9, '/', '-', '.', ' ', '$', '(', ')', '_', and '+'."
                    .to_string()
            ))
        );
        assert_eq!(
            analyze("//bar/:baz"),
            Err(LabelError(
                "//bar/:baz must be a legal label; package names may not end with '/'.".to_string()
            ))
        );

        assert_eq!(analyze("@repo//foo/bar")?.package(), Some("foo/bar"));
        assert_eq!(analyze("//foo/bar")?.package(), Some("foo/bar"));
        assert_eq!(
            analyze("foo/bar"),
            Err(LabelError(
                "foo/bar must be a legal label; relative packages are not permitted.".to_string()
            ))
        );

        assert_eq!(analyze("@repo//foo")?.package(), Some("foo"));
        assert_eq!(analyze("//foo")?.package(), Some("foo"));
        assert_eq!(
            analyze("foo"),
            Err(LabelError(
                "foo must be a legal label; relative packages are not permitted.".to_string()
            ))
        );

        Ok(())
    }

    #[test]
    fn test_name_parsing() -> Result<()> {
        assert_eq!(analyze("//foo:baz")?.name(), "baz");
        assert_eq!(analyze("//foo:baz/qux")?.name(), "baz/qux");
        assert_eq!(analyze(":baz/qux")?.name(), "baz/qux");

        assert_eq!(
            analyze("::baz/qux"),
            Err(LabelError(
                "::baz/qux must be a legal label; target names may not contain with ':'."
                    .to_string()
            ))
        );

        assert_eq!(
            analyze("//bar:"),
            Err(LabelError(
                "//bar: must be a legal label; empty target name.".to_string()
            ))
        );
        assert_eq!(analyze("//foo")?.name(), "foo");

        assert_eq!(
            analyze("//bar:/foo"),
            Err(LabelError(
                "//bar:/foo must be a legal label; target names may not start with '/'."
                    .to_string()
            ))
        );

        assert_eq!(analyze("@repo//foo/bar")?.name(), "bar");
        assert_eq!(analyze("//foo/bar")?.name(), "bar");
        assert_eq!(
            analyze("foo/bar"),
            Err(LabelError(
                "foo/bar must be a legal label; relative packages are not permitted.".to_string()
            ))
        );

        assert_eq!(analyze("@repo//foo")?.name(), "foo");
        assert_eq!(analyze("//foo")?.name(), "foo");
        assert_eq!(
            analyze("foo"),
            Err(LabelError(
                "foo must be a legal label; relative packages are not permitted.".to_string()
            ))
        );

        assert_eq!(
            analyze("@repo")?,
            Label::Absolute {
                repository: Some(Repository::Apparent("@repo")),
                package_name: "",
                target_name: "repo",
            },
        );

        assert_eq!(
            analyze("@"),
            Err(LabelError(
                "@ must be a legal label; invalid target name: empty target name".to_string()
            )),
        );

        Ok(())
    }
}
