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

use std::convert::{TryFrom, TryInto};

use tinyjson::JsonValue;

use crate::output::{LineOutput, LineResult};

#[derive(Debug, Copy, Clone)]
pub(crate) enum ErrorFormat {
    Json,
    Rendered,
}

impl Default for ErrorFormat {
    fn default() -> Self {
        Self::Rendered
    }
}

fn get_key(value: &JsonValue, key: &str) -> Option<String> {
    if let JsonValue::Object(map) = value {
        if let JsonValue::String(s) = map.get(key)? {
            Some(s.clone())
        } else {
            None
        }
    } else {
        None
    }
}

#[derive(Debug)]
enum RustcMessage {
    Emit(String),
    Message(String),
}

impl TryFrom<JsonValue> for RustcMessage {
    type Error = ();
    fn try_from(val: JsonValue) -> Result<Self, Self::Error> {
        if let Some(emit) = get_key(&val, "emit") {
            return Ok(Self::Emit(emit));
        }
        if let Some(rendered) = get_key(&val, "rendered") {
            return Ok(Self::Message(rendered));
        }
        Err(())
    }
}

/// process_rustc_json takes an output line from rustc configured with
/// --error-format=json, parses the json and returns the appropriate output
/// according to the original --error-format supplied.
/// Only messages are returned, emits are ignored.
/// Retuns an errors if parsing json fails.
pub(crate) fn process_json(line: String, error_format: ErrorFormat) -> LineResult {
    let parsed: JsonValue = line
        .parse()
        .map_err(|_| "error parsing rustc output as json".to_owned())?;
    Ok(match parsed.try_into() {
        Ok(RustcMessage::Message(rendered)) => {
            output_based_on_error_format(line, rendered, error_format)
        }
        _ => LineOutput::Skip,
    })
}

/// stop_on_rmeta_completion parses the json output of rustc in the same way
/// process_rustc_json does. In addition, it will signal to stop when metadata
/// is emitted so the compiler can be terminated.
/// This is used to implement pipelining in rules_rust, please see
/// https://internals.rust-lang.org/t/evaluating-pipelined-rustc-compilation/10199
/// Retuns an error if parsing json fails.
/// TODO: pass a function to handle the emit event and merge with process_json
pub(crate) fn stop_on_rmeta_completion(
    line: String,
    error_format: ErrorFormat,
    kill: &mut bool,
) -> LineResult {
    let parsed: JsonValue = line
        .parse()
        .map_err(|_| "error parsing rustc output as json".to_owned())?;
    Ok(match parsed.try_into() {
        Ok(RustcMessage::Emit(emit)) if emit == "metadata" => {
            *kill = true;
            LineOutput::Terminate
        }
        Ok(RustcMessage::Message(rendered)) => {
            output_based_on_error_format(line, rendered, error_format)
        }
        _ => LineOutput::Skip,
    })
}

fn output_based_on_error_format(
    line: String,
    rendered: String,
    error_format: ErrorFormat,
) -> LineOutput {
    match error_format {
        // If the output should be json, we just forward the messages as-is
        // using `line`.
        ErrorFormat::Json => LineOutput::Message(line),
        // Otherwise we return the rendered field.
        ErrorFormat::Rendered => LineOutput::Message(rendered),
    }
}
