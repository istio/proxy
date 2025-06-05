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

use std::env::args;
use std::error;
use std::fs::File;
use std::path::Path;
use std::process::Command;

// A simple wrapper around a binary to ensure we always create some outputs
// Optional outputs are not available in Skylark :(
// Syntax: $0 output1 output2 ... -- program  [arg1...argn]
fn ensure() -> Result<i32, Box<dyn error::Error>> {
    let index = args().position(|a| a == "--").ok_or("no --")?;
    let optional_outputs = args().take(index).collect::<Vec<String>>();
    let exe = args().nth(index + 1).ok_or("no exe")?;
    let exe_args = args().skip(index + 2).collect::<Vec<String>>();
    if exe_args.is_empty() {
        return Err("no exe args".into());
    }
    match Command::new(exe).args(exe_args).status()?.code() {
        Some(code) => {
            if code == 0 {
                for out in optional_outputs {
                    if !Path::new(&out).exists() {
                        let _ = File::create(out)?;
                    }
                }
            }
            Ok(code)
        }
        None => Err("process killed".into()),
    }
}

fn main() {
    std::process::exit(match ensure() {
        Ok(exit_code) => exit_code,
        Err(e) => {
            println!("Usage: [optional_output1...optional_outputN] -- program [arg1...argn]");
            println!("{:?}", args());
            println!("{e:?}");
            -1
        }
    });
}
