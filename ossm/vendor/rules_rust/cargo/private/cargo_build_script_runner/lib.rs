// Copyright 2018 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Parse the output of a cargo build.rs script and generate a list of flags and
//! environment variable for the build.
use std::io::{BufRead, BufReader, Read};
use std::process::{Command, Output};

pub mod cargo_manifest_dir;

#[derive(Debug, PartialEq, Eq)]
pub struct CompileAndLinkFlags {
    pub compile_flags: String,
    pub link_flags: String,
    pub link_search_paths: String,
}

/// Enum containing all the considered return value from the script
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BuildScriptOutput {
    /// cargo::rustc-link-lib
    LinkLib(String),
    /// cargo::rustc-link-search
    LinkSearch(String),
    /// cargo::rustc-cfg
    Cfg(String),
    /// cargo::rustc-flags
    Flags(String),
    /// cargo::rustc-link-arg
    LinkArg(String),
    /// cargo::rustc-env
    Env(String),
    /// cargo::VAR=VALUE
    DepEnv(String),
}

impl BuildScriptOutput {
    /// Converts a line into a [BuildScriptOutput] enum.
    ///
    /// Examples
    /// ```rust
    /// assert_eq!(BuildScriptOutput::new("cargo::rustc-link-lib=lib"), Some(BuildScriptOutput::LinkLib("lib".to_owned())));
    /// ```
    fn new(line: &str) -> Option<BuildScriptOutput> {
        let split = line.splitn(2, '=').collect::<Vec<_>>();
        if split.len() <= 1 {
            // Not a cargo directive.
            return None;
        }
        let param = split[1].trim().to_owned();
        let cargo_instruction_name = {
            if split[0].starts_with("cargo::") {
                &split[0][7..]
            } else if split[0].starts_with("cargo:") {
                &split[0][6..]
            } else {
                // Not a cargo directive.
                return None;
            }
        };

        match cargo_instruction_name {
            "rustc-link-lib" => Some(BuildScriptOutput::LinkLib(param)),
            "rustc-link-search" => Some(BuildScriptOutput::LinkSearch(param)),
            "rustc-cfg" => Some(BuildScriptOutput::Cfg(param)),
            "rustc-flags" => Some(BuildScriptOutput::Flags(param)),
            "rustc-link-arg" => Some(BuildScriptOutput::LinkArg(param)),
            "rustc-env" => Some(BuildScriptOutput::Env(param)),
            "rerun-if-changed" | "rerun-if-env-changed" =>
            // Ignored because Bazel will re-run if those change all the time.
            {
                None
            }
            "warning" => {
                eprint!("Build Script Warning: {}", split[1]);
                None
            }
            "rustc-cdylib-link-arg" | "rustc-link-arg-bin" | "rustc-link-arg-bins" => {
                // cargo::rustc-cdylib-link-arg=FLAG — Passes custom flags to a linker for cdylib crates.
                // cargo::rustc-link-arg-bin=BIN=FLAG – Passes custom flags to a linker for the binary BIN.
                // cargo::rustc-link-arg-bins=FLAG – Passes custom flags to a linker for binaries.
                eprint!(
                    "Warning: build script returned unsupported directive `{}`",
                    split[0]
                );
                None
            }
            _ => {
                // cargo::KEY=VALUE — Metadata, used by links scripts.
                Some(BuildScriptOutput::DepEnv(format!(
                    "{}={}",
                    cargo_instruction_name.to_uppercase().replace('-', "_"),
                    param
                )))
            }
        }
    }

    /// Converts a [BufReader] into a vector of [BuildScriptOutput] enums.
    fn outputs_from_reader<T: Read>(mut reader: BufReader<T>) -> Vec<BuildScriptOutput> {
        let mut result = Vec::<BuildScriptOutput>::new();
        let mut buf = Vec::new();
        while reader
            .read_until(b'\n', &mut buf)
            .expect("Cannot read line")
            != 0
        {
            // like cargo, ignore any lines that are not valid utf8
            if let Ok(line) = String::from_utf8(buf.clone()) {
                if let Some(bso) = BuildScriptOutput::new(&line) {
                    result.push(bso);
                }
            }
            buf.clear();
        }
        result
    }

    /// Take a [Command], execute it and converts its input into a vector of [BuildScriptOutput]
    pub fn outputs_from_command(
        cmd: &mut Command,
    ) -> Result<(Vec<BuildScriptOutput>, Output), Output> {
        let child_output = cmd
            .output()
            .unwrap_or_else(|e| panic!("Unable to start command:\n{:#?}\n{:?}", cmd, e));
        if child_output.status.success() {
            let reader = BufReader::new(child_output.stdout.as_slice());
            let output = Self::outputs_from_reader(reader);
            Ok((output, child_output))
        } else {
            Err(child_output)
        }
    }

    /// Convert a vector of [BuildScriptOutput] into a list of environment variables.
    pub fn outputs_to_env(outputs: &[BuildScriptOutput], exec_root: &str) -> String {
        outputs
            .iter()
            .filter_map(|x| {
                if let BuildScriptOutput::Env(env) = x {
                    Some(Self::escape_for_serializing(Self::redact_exec_root(
                        env, exec_root,
                    )))
                } else {
                    None
                }
            })
            .collect::<Vec<_>>()
            .join("\n")
    }

    /// Convert a vector of [BuildScriptOutput] into a list of dependencies environment variables.
    pub fn outputs_to_dep_env(
        outputs: &[BuildScriptOutput],
        crate_links: &str,
        exec_root: &str,
    ) -> String {
        let prefix = format!("DEP_{}_", crate_links.replace('-', "_").to_uppercase());
        outputs
            .iter()
            .filter_map(|x| {
                if let BuildScriptOutput::DepEnv(env) = x {
                    Some(format!(
                        "{}{}",
                        prefix,
                        Self::escape_for_serializing(Self::redact_exec_root(env, exec_root))
                    ))
                } else {
                    None
                }
            })
            .collect::<Vec<_>>()
            .join("\n")
    }

    /// Convert a vector of [BuildScriptOutput] into a flagfile.
    pub fn outputs_to_flags(outputs: &[BuildScriptOutput], exec_root: &str) -> CompileAndLinkFlags {
        let mut compile_flags = Vec::new();
        let mut link_flags = Vec::new();
        let mut link_search_paths = Vec::new();

        for flag in outputs {
            match flag {
                BuildScriptOutput::Cfg(e) => compile_flags.push(format!("--cfg={e}")),
                BuildScriptOutput::Flags(e) => compile_flags.push(e.to_owned()),
                BuildScriptOutput::LinkArg(e) => compile_flags.push(format!("-Clink-arg={e}")),
                BuildScriptOutput::LinkLib(e) => link_flags.push(format!("-l{e}")),
                BuildScriptOutput::LinkSearch(e) => link_search_paths.push(format!("-L{e}")),
                _ => {}
            }
        }

        CompileAndLinkFlags {
            compile_flags: compile_flags.join("\n"),
            link_flags: Self::redact_exec_root(&link_flags.join("\n"), exec_root),
            link_search_paths: Self::redact_exec_root(&link_search_paths.join("\n"), exec_root),
        }
    }

    fn redact_exec_root(value: &str, exec_root: &str) -> String {
        value.replace(exec_root, "${pwd}")
    }

    // The process-wrapper treats trailing backslashes as escapes for following newlines.
    // If the env var ends with a backslash (and accordingly doesn't have a following newline),
    // escape it so that it doesn't get turned into a newline by the process-wrapper.
    //
    // Note that this code doesn't handle newlines in strings - that's because Cargo treats build
    // script output as single-line-oriented, so stops processing at the end of a line regardless.
    fn escape_for_serializing(mut value: String) -> String {
        if value.ends_with('\\') {
            value.push('\\');
        }
        value
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    fn from_read_buffer_to_env_and_flags_test_impl(buff: Cursor<&str>) {
        let reader = BufReader::new(buff);
        let result = BuildScriptOutput::outputs_from_reader(reader);
        assert_eq!(result.len(), 13);
        assert_eq!(result[0], BuildScriptOutput::LinkLib("sdfsdf".to_owned()));
        assert_eq!(result[1], BuildScriptOutput::Env("FOO=BAR".to_owned()));
        assert_eq!(
            result[2],
            BuildScriptOutput::LinkSearch("/some/absolute/path/bleh".to_owned())
        );
        assert_eq!(result[3], BuildScriptOutput::Env("BAR=FOO".to_owned()));
        assert_eq!(result[4], BuildScriptOutput::Flags("-Lblah".to_owned()));
        assert_eq!(
            result[5],
            BuildScriptOutput::Cfg("feature=awesome".to_owned())
        );
        assert_eq!(
            result[6],
            BuildScriptOutput::DepEnv("VERSION=123".to_owned())
        );
        assert_eq!(
            result[7],
            BuildScriptOutput::DepEnv("VERSION_NUMBER=1010107f".to_owned())
        );
        assert_eq!(
            result[9],
            BuildScriptOutput::Env("SOME_PATH=/some/absolute/path/beep".to_owned())
        );
        assert_eq!(
            result[10],
            BuildScriptOutput::LinkArg("-weak_framework".to_owned())
        );
        assert_eq!(result[11], BuildScriptOutput::LinkArg("Metal".to_owned()));
        assert_eq!(
            result[12],
            BuildScriptOutput::Env("no_trailing_newline=true".to_owned())
        );
        assert_eq!(
            BuildScriptOutput::outputs_to_dep_env(&result, "ssh2", "/some/absolute/path"),
            "DEP_SSH2_VERSION=123\nDEP_SSH2_VERSION_NUMBER=1010107f\nDEP_SSH2_INCLUDE_PATH=${pwd}/include".to_owned()
        );
        assert_eq!(
            BuildScriptOutput::outputs_to_env(&result, "/some/absolute/path"),
            "FOO=BAR\nBAR=FOO\nSOME_PATH=${pwd}/beep\nno_trailing_newline=true".to_owned()
        );
        assert_eq!(
            BuildScriptOutput::outputs_to_flags(&result, "/some/absolute/path"),
            CompileAndLinkFlags {
                // -Lblah was output as a rustc-flags, so even though it probably _should_ be a link
                // flag, we don't treat it like one.
                compile_flags:
                    "-Lblah\n--cfg=feature=awesome\n-Clink-arg=-weak_framework\n-Clink-arg=Metal"
                        .to_owned(),
                link_flags: "-lsdfsdf".to_owned(),
                link_search_paths: "-L${pwd}/bleh".to_owned(),
            }
        );
    }

    #[test]
    fn test_from_read_buffer_to_env_and_flags() {
        let buff = Cursor::new(
            "
cargo::rustc-link-lib=sdfsdf
cargo::rustc-env=FOO=BAR
cargo::rustc-link-search=/some/absolute/path/bleh
cargo::rustc-env=BAR=FOO
cargo::rustc-flags=-Lblah
cargo::rerun-if-changed=ignored
cargo::rustc-cfg=feature=awesome
cargo::version=123
cargo::version_number=1010107f
cargo::include_path=/some/absolute/path/include
cargo::rustc-env=SOME_PATH=/some/absolute/path/beep
cargo::rustc-link-arg=-weak_framework
cargo::rustc-link-arg=Metal
cargo::rustc-env=no_trailing_newline=true
non-cargo-prefixes::are-ignored=true
non-assignment-instructions-are-ignored",
        );
        from_read_buffer_to_env_and_flags_test_impl(buff);
    }

    /// Demonstrate that the old style single colon flags are all parsable
    #[test]
    fn test_legacy_from_read_buffer_to_env_and_flags() {
        let buff = Cursor::new(
            "
cargo:rustc-link-lib=sdfsdf
cargo:rustc-env=FOO=BAR
cargo:rustc-link-search=/some/absolute/path/bleh
cargo:rustc-env=BAR=FOO
cargo:rustc-flags=-Lblah
cargo:rerun-if-changed=ignored
cargo:rustc-cfg=feature=awesome
cargo:version=123
cargo:version_number=1010107f
cargo:include_path=/some/absolute/path/include
cargo:rustc-env=SOME_PATH=/some/absolute/path/beep
cargo:rustc-link-arg=-weak_framework
cargo:rustc-link-arg=Metal
cargo:rustc-env=no_trailing_newline=true
non-cargo-prefixes:are-ignored=true
non-assignment-instructions-are-ignored",
        );
        from_read_buffer_to_env_and_flags_test_impl(buff);
    }

    #[test]
    fn invalid_utf8() {
        let buff = Cursor::new(
            b"
cargo::rustc-env=valid1=1
cargo::rustc-env=invalid=\xc3\x28
cargo::rustc-env=valid2=2
",
        );
        let reader = BufReader::new(buff);
        let result = BuildScriptOutput::outputs_from_reader(reader);
        assert_eq!(result.len(), 2);
        assert_eq!(
            &BuildScriptOutput::outputs_to_env(&result, "/some/absolute/path"),
            "valid1=1\nvalid2=2"
        );
    }

    /// Demonstrate that the old style single colon flags are all parsable
    #[test]
    fn invalid_utf8_legacy() {
        let buff = Cursor::new(
            b"
cargo:rustc-env=valid1=1
cargo:rustc-env=invalid=\xc3\x28
cargo:rustc-env=valid2=2
",
        );
        let reader = BufReader::new(buff);
        let result = BuildScriptOutput::outputs_from_reader(reader);
        assert_eq!(result.len(), 2);
        assert_eq!(
            &BuildScriptOutput::outputs_to_env(&result, "/some/absolute/path"),
            "valid1=1\nvalid2=2"
        );
    }
}
