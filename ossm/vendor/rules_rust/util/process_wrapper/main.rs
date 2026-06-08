// Copyright 2020 The Bazel Authors. All rights reserved.
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

mod flags;
mod options;
mod output;
mod rustc;
mod util;

use std::collections::HashMap;
use std::fmt;
use std::fs::{copy, OpenOptions};
use std::io;
use std::process::{exit, Command, ExitStatus, Stdio};

use tinyjson::JsonValue;

use crate::options::options;
use crate::output::{process_output, LineOutput};
use crate::rustc::ErrorFormat;

#[cfg(windows)]
fn status_code(status: ExitStatus, was_killed: bool) -> i32 {
    // On windows, there's no good way to know if the process was killed by a signal.
    // If we killed the process, we override the code to signal success.
    if was_killed {
        0
    } else {
        status.code().unwrap_or(1)
    }
}

#[cfg(not(windows))]
fn status_code(status: ExitStatus, was_killed: bool) -> i32 {
    // On unix, if code is None it means that the process was killed by a signal.
    // https://doc.rust-lang.org/std/process/struct.ExitStatus.html#method.success
    match status.code() {
        Some(code) => code,
        // If we killed the process, we expect None here
        None if was_killed => 0,
        // Otherwise it's some unexpected signal
        None => 1,
    }
}

#[derive(Debug)]
struct ProcessWrapperError(String);

impl fmt::Display for ProcessWrapperError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "process wrapper error: {}", self.0)
    }
}

impl std::error::Error for ProcessWrapperError {}

macro_rules! debug_log {
    ($($arg:tt)*) => {
        if std::env::var_os("RULES_RUST_PROCESS_WRAPPER_DEBUG").is_some() {
            eprintln!($($arg)*);
        }
    };
}

fn json_warning(line: &str) -> JsonValue {
    JsonValue::Object(HashMap::from([
        (
            "$message_type".to_string(),
            JsonValue::String("diagnostic".to_string()),
        ),
        ("message".to_string(), JsonValue::String(line.to_string())),
        ("code".to_string(), JsonValue::Null),
        (
            "level".to_string(),
            JsonValue::String("warning".to_string()),
        ),
        ("spans".to_string(), JsonValue::Array(Vec::new())),
        ("children".to_string(), JsonValue::Array(Vec::new())),
        ("rendered".to_string(), JsonValue::String(line.to_string())),
    ]))
}

fn process_line(
    mut line: String,
    quit_on_rmeta: bool,
    format: ErrorFormat,
    metadata_emitted: &mut bool,
) -> Result<LineOutput, String> {
    // LLVM can emit lines that look like the following, and these will be interspersed
    // with the regular JSON output. Arguably, rustc should be fixed not to emit lines
    // like these (or to convert them to JSON), but for now we convert them to JSON
    // ourselves.
    if line.contains("is not a recognized feature for this target (ignoring feature)")
        || line.starts_with(" WARN ")
    {
        if let Ok(json_str) = json_warning(&line).stringify() {
            line = json_str;
        } else {
            return Ok(LineOutput::Skip);
        }
    }
    if quit_on_rmeta {
        rustc::stop_on_rmeta_completion(line, format, metadata_emitted)
    } else {
        rustc::process_json(line, format)
    }
}

fn main() -> Result<(), ProcessWrapperError> {
    let opts = options().map_err(|e| ProcessWrapperError(e.to_string()))?;

    let mut command = Command::new(opts.executable);
    command
        .args(opts.child_arguments)
        .env_clear()
        .envs(opts.child_environment)
        .stdout(if let Some(stdout_file) = opts.stdout_file {
            OpenOptions::new()
                .create(true)
                .truncate(true)
                .write(true)
                .open(stdout_file)
                .map_err(|e| ProcessWrapperError(format!("unable to open stdout file: {}", e)))?
                .into()
        } else {
            Stdio::inherit()
        })
        .stderr(Stdio::piped());
    debug_log!("{:#?}", command);
    let mut child = command
        .spawn()
        .map_err(|e| ProcessWrapperError(format!("failed to spawn child process: {}", e)))?;

    let mut stderr: Box<dyn io::Write> = if let Some(stderr_file) = opts.stderr_file {
        Box::new(
            OpenOptions::new()
                .create(true)
                .truncate(true)
                .write(true)
                .open(stderr_file)
                .map_err(|e| ProcessWrapperError(format!("unable to open stderr file: {}", e)))?,
        )
    } else {
        Box::new(io::stderr())
    };

    let mut child_stderr = child.stderr.take().ok_or(ProcessWrapperError(
        "unable to get child stderr".to_string(),
    ))?;

    let mut output_file: Option<std::fs::File> = if let Some(output_file_name) = opts.output_file {
        Some(
            OpenOptions::new()
                .create(true)
                .truncate(true)
                .write(true)
                .open(output_file_name)
                .map_err(|e| ProcessWrapperError(format!("Unable to open output_file: {}", e)))?,
        )
    } else {
        None
    };

    let mut was_killed = false;
    let result = if let Some(format) = opts.rustc_output_format {
        let quit_on_rmeta = opts.rustc_quit_on_rmeta;
        // Process json rustc output and kill the subprocess when we get a signal
        // that we emitted a metadata file.
        let mut me = false;
        let metadata_emitted = &mut me;
        let result = process_output(
            &mut child_stderr,
            stderr.as_mut(),
            output_file.as_mut(),
            move |line| process_line(line, quit_on_rmeta, format, metadata_emitted),
        );
        if me {
            // If recv returns Ok(), a signal was sent in this channel so we should terminate the child process.
            // We can safely ignore the Result from kill() as we don't care if the process already terminated.
            let _ = child.kill();
            was_killed = true;
        }
        result
    } else {
        // Process output normally by forwarding stderr
        process_output(
            &mut child_stderr,
            stderr.as_mut(),
            output_file.as_mut(),
            move |line| Ok(LineOutput::Message(line)),
        )
    };
    result.map_err(|e| ProcessWrapperError(format!("failed to process stderr: {}", e)))?;

    let status = child
        .wait()
        .map_err(|e| ProcessWrapperError(format!("failed to wait for child process: {}", e)))?;
    // If the child process is rustc and is killed after metadata generation, that's also a success.
    let code = status_code(status, was_killed);
    let success = code == 0;
    if success {
        if let Some(tf) = opts.touch_file {
            OpenOptions::new()
                .create(true)
                .truncate(true)
                .write(true)
                .open(tf)
                .map_err(|e| ProcessWrapperError(format!("failed to create touch file: {}", e)))?;
        }
        if let Some((copy_source, copy_dest)) = opts.copy_output {
            copy(&copy_source, &copy_dest).map_err(|e| {
                ProcessWrapperError(format!(
                    "failed to copy {} into {}: {}",
                    copy_source, copy_dest, e
                ))
            })?;
        }
    }

    exit(code)
}

#[cfg(test)]
mod test {
    use super::*;

    fn parse_json(json_str: &str) -> Result<JsonValue, String> {
        json_str.parse::<JsonValue>().map_err(|e| e.to_string())
    }

    #[test]
    fn test_process_line_diagnostic_json() -> Result<(), String> {
        let mut metadata_emitted = false;
        let LineOutput::Message(msg) = process_line(
            r#"
                {
                    "$message_type": "diagnostic",
                    "rendered": "Diagnostic message"
                }
            "#
            .to_string(),
            false,
            ErrorFormat::Json,
            &mut metadata_emitted,
        )?
        else {
            return Err("Expected a LineOutput::Message".to_string());
        };
        assert_eq!(
            parse_json(&msg)?,
            parse_json(
                r#"
                {
                    "$message_type": "diagnostic",
                    "rendered": "Diagnostic message"
                }
            "#
            )?
        );
        Ok(())
    }

    #[test]
    fn test_process_line_diagnostic_rendered() -> Result<(), String> {
        let mut metadata_emitted = false;
        let LineOutput::Message(msg) = process_line(
            r#"
                {
                    "$message_type": "diagnostic",
                    "rendered": "Diagnostic message"
                }
            "#
            .to_string(),
            /*quit_on_rmeta=*/ false,
            ErrorFormat::Rendered,
            &mut metadata_emitted,
        )?
        else {
            return Err("Expected a LineOutput::Message".to_string());
        };
        assert_eq!(msg, "Diagnostic message");
        Ok(())
    }

    #[test]
    fn test_process_line_noise() -> Result<(), String> {
        let mut metadata_emitted = false;
        for text in [
            "'+zaamo' is not a recognized feature for this target (ignoring feature)",
            " WARN rustc_errors::emitter Invalid span...",
        ] {
            let LineOutput::Message(msg) = process_line(
                text.to_string(),
                /*quit_on_rmeta=*/ false,
                ErrorFormat::Json,
                &mut metadata_emitted,
            )?
            else {
                return Err("Expected a LineOutput::Message".to_string());
            };
            assert_eq!(
                parse_json(&msg)?,
                parse_json(&format!(
                    r#"{{
                        "$message_type": "diagnostic",
                        "message": "{0}",
                        "code": null,
                        "level": "warning",
                        "spans": [],
                        "children": [],
                        "rendered": "{0}"
                    }}"#,
                    text
                ))?
            );
        }
        Ok(())
    }

    #[test]
    fn test_process_line_emit_link() -> Result<(), String> {
        let mut metadata_emitted = false;
        assert!(matches!(
            process_line(
                r#"
                {
                    "$message_type": "artifact",
                    "emit": "link"
                }
            "#
                .to_string(),
                /*quit_on_rmeta=*/ true,
                ErrorFormat::Rendered,
                &mut metadata_emitted,
            )?,
            LineOutput::Skip
        ));
        assert!(!metadata_emitted);
        Ok(())
    }

    #[test]
    fn test_process_line_emit_metadata() -> Result<(), String> {
        let mut metadata_emitted = false;
        assert!(matches!(
            process_line(
                r#"
                {
                    "$message_type": "artifact",
                    "emit": "metadata"
                }
            "#
                .to_string(),
                /*quit_on_rmeta=*/ true,
                ErrorFormat::Rendered,
                &mut metadata_emitted,
            )?,
            LineOutput::Terminate
        ));
        assert!(metadata_emitted);
        Ok(())
    }
}
