//! Utilities for parsing [Args](https://bazel.build/rules/lib/builtins/Args.html) param files.

use std::path::Path;

/// The format for an [Args param file[(https://bazel.build/rules/lib/builtins/Args.html#set_param_file_format).
#[derive(Debug)]
pub enum ActionArgsFormat {
    /// Each item (argument name or value) is written verbatim to the param
    /// file with a newline character following it.
    Multiline,

    /// Same as [Self::Multiline], but the items are shell-quoted.
    Shell,

    /// Same as [Self::Multiline], but (1) only flags (beginning with '--')
    /// are written to the param file, and (2) the values of the flags, if
    /// any, are written on the same line with a '=' separator. This is the
    /// format expected by the Abseil flags library.
    FlagPerLine,
}

impl Default for ActionArgsFormat {
    fn default() -> Self {
        Self::Shell
    }
}

/// Parsed [`ctx.action.args`](https://bazel.build/rules/lib/builtins/Args.html) params.
type ActionArgv = Vec<String>;

/// Parse an [Args](https://bazel.build/rules/lib/builtins/Args.html) param file string into an argv list.
pub fn parse_args_with_fmt(text: String, fmt: ActionArgsFormat) -> ActionArgv {
    text.lines()
        .map(|s| match fmt {
            ActionArgsFormat::Shell => {
                if s.starts_with('\'') && s.ends_with('\'') {
                    s[1..s.len() - 1].to_owned()
                } else {
                    s.to_owned()
                }
            }
            _ => s.to_owned(),
        })
        .collect()
}

/// Parse an [Args](https://bazel.build/rules/lib/builtins/Args.html) param file string into an argv list.
pub fn parse_args(text: String) -> ActionArgv {
    parse_args_with_fmt(text, ActionArgsFormat::default())
}

/// Parse an [Args](https://bazel.build/rules/lib/builtins/Args.html) param file into an argv list.
pub fn try_parse_args_with_fmt(
    path: &Path,
    fmt: ActionArgsFormat,
) -> Result<ActionArgv, std::io::Error> {
    let text = std::fs::read_to_string(path)?;
    Ok(parse_args_with_fmt(text, fmt))
}

/// Parse an [Args](https://bazel.build/rules/lib/builtins/Args.html) param file into an argv list.
pub fn try_parse_args(path: &Path) -> Result<ActionArgv, std::io::Error> {
    let text = std::fs::read_to_string(path)?;
    Ok(parse_args(text))
}

#[cfg(test)]
mod test {
    use std::path::PathBuf;

    use super::*;

    const TEST_ARGS: [&str; 5] = ["foo", "-bar", "'baz'", "'--qux=quux'", "--quuz='corge'"];

    #[test]
    fn test_multiline_string() {
        let text = TEST_ARGS.join("\n");

        let args = parse_args_with_fmt(text, ActionArgsFormat::Multiline);
        assert_eq!(
            vec!["foo", "-bar", "'baz'", "'--qux=quux'", "--quuz='corge'"],
            args
        )
    }

    #[test]
    fn test_shell_string() {
        let text = TEST_ARGS.join("\n");

        let args = parse_args_with_fmt(text, ActionArgsFormat::Shell);
        assert_eq!(
            vec!["foo", "-bar", "baz", "--qux=quux", "--quuz='corge'"],
            args
        )
    }

    #[test]
    fn test_flag_per_line_string() {
        let text = TEST_ARGS.join("\n");

        let args = parse_args_with_fmt(text, ActionArgsFormat::FlagPerLine);
        assert_eq!(
            vec!["foo", "-bar", "'baz'", "'--qux=quux'", "--quuz='corge'"],
            args
        )
    }

    #[test]
    fn test_from_file() {
        let text = TEST_ARGS.join("\n");

        let test_tempdir = PathBuf::from(std::env::var("TEST_TMPDIR").unwrap());
        let test_file = test_tempdir.join("test_from_file.txt");

        assert!(try_parse_args(&test_file).is_err());

        std::fs::write(&test_file, text).unwrap();

        let args = try_parse_args(&test_file).unwrap();
        assert_eq!(
            vec!["foo", "-bar", "baz", "--qux=quux", "--quuz='corge'"],
            args
        )
    }
}
