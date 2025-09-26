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

use std::collections::{BTreeMap, HashSet};
use std::error::Error;
use std::fmt;
use std::fmt::Write;
use std::iter::Peekable;
use std::mem::take;

#[derive(Debug, Clone)]
pub(crate) enum FlagParseError {
    UnknownFlag(String),
    ValueMissing(String),
    ProvidedMultipleTimes(String),
    ProgramNameMissing,
}

impl fmt::Display for FlagParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::UnknownFlag(ref flag) => write!(f, "unknown flag \"{flag}\""),
            Self::ValueMissing(ref flag) => write!(f, "flag \"{flag}\" missing parameter(s)"),
            Self::ProvidedMultipleTimes(ref flag) => {
                write!(f, "flag \"{flag}\" can only appear once")
            }
            Self::ProgramNameMissing => {
                write!(f, "program name (argv[0]) missing")
            }
        }
    }
}
impl Error for FlagParseError {}

struct FlagDef<'a, T> {
    name: String,
    help: String,
    output_storage: &'a mut Option<T>,
}

impl<T> fmt::Display for FlagDef<'_, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}\t{}", self.name, self.help)
    }
}

impl<T> fmt::Debug for FlagDef<'_, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("FlagDef")
            .field("name", &self.name)
            .field("help", &self.help)
            .finish()
    }
}

#[derive(Debug)]
pub(crate) struct Flags<'a> {
    single: BTreeMap<String, FlagDef<'a, String>>,
    repeated: BTreeMap<String, FlagDef<'a, Vec<String>>>,
}

#[derive(Debug)]
pub(crate) enum ParseOutcome {
    Help(String),
    Parsed(Vec<String>),
}

impl<'a> Flags<'a> {
    pub(crate) fn new() -> Flags<'a> {
        Flags {
            single: BTreeMap::new(),
            repeated: BTreeMap::new(),
        }
    }

    pub(crate) fn define_flag(
        &mut self,
        name: impl Into<String>,
        help: impl Into<String>,
        output_storage: &'a mut Option<String>,
    ) {
        let name = name.into();
        if self.repeated.contains_key(&name) {
            panic!("argument \"{}\" already defined as repeated flag", name)
        }
        self.single.insert(
            name.clone(),
            FlagDef::<'a, String> {
                name,
                help: help.into(),
                output_storage,
            },
        );
    }

    pub(crate) fn define_repeated_flag(
        &mut self,
        name: impl Into<String>,
        help: impl Into<String>,
        output_storage: &'a mut Option<Vec<String>>,
    ) {
        let name = name.into();
        if self.single.contains_key(&name) {
            panic!("argument \"{}\" already defined as flag", name)
        }
        self.repeated.insert(
            name.clone(),
            FlagDef::<'a, Vec<String>> {
                name,
                help: help.into(),
                output_storage,
            },
        );
    }

    fn help(&self, program_name: String) -> String {
        let single = self.single.values().map(|fd| fd.to_string());
        let repeated = self.repeated.values().map(|fd| fd.to_string());
        let mut all: Vec<String> = single.chain(repeated).collect();
        all.sort();

        let mut help_text = String::new();
        writeln!(
            &mut help_text,
            "Help for {program_name}: [options] -- [extra arguments]"
        )
        .unwrap();
        for line in all {
            writeln!(&mut help_text, "\t{line}").unwrap();
        }
        help_text
    }

    pub(crate) fn parse(mut self, argv: Vec<String>) -> Result<ParseOutcome, FlagParseError> {
        let mut argv_iter = argv.into_iter().peekable();
        let program_name = argv_iter.next().ok_or(FlagParseError::ProgramNameMissing)?;

        // To check if a non-repeated flag has been set already.
        let mut seen_single_flags = HashSet::<String>::new();

        while let Some(flag) = argv_iter.next() {
            if flag == "--help" {
                return Ok(ParseOutcome::Help(self.help(program_name)));
            }
            if !flag.starts_with("--") {
                return Err(FlagParseError::UnknownFlag(flag));
            }
            let mut args = consume_args(&flag, &mut argv_iter);
            if flag == "--" {
                return Ok(ParseOutcome::Parsed(args));
            }
            if args.is_empty() {
                return Err(FlagParseError::ValueMissing(flag.clone()));
            }
            if let Some(flag_def) = self.single.get_mut(&flag) {
                if args.len() > 1 || seen_single_flags.contains(&flag) {
                    return Err(FlagParseError::ProvidedMultipleTimes(flag.clone()));
                }
                let arg = args.first_mut().unwrap();
                seen_single_flags.insert(flag);
                *flag_def.output_storage = Some(take(arg));
                continue;
            }
            if let Some(flag_def) = self.repeated.get_mut(&flag) {
                flag_def
                    .output_storage
                    .get_or_insert_with(Vec::new)
                    .append(&mut args);
                continue;
            }
            return Err(FlagParseError::UnknownFlag(flag));
        }
        Ok(ParseOutcome::Parsed(vec![]))
    }
}

fn consume_args<I: Iterator<Item = String>>(
    flag: &str,
    argv_iter: &mut Peekable<I>,
) -> Vec<String> {
    if flag == "--" {
        // If we have found --, the rest of the iterator is just returned as-is.
        argv_iter.collect()
    } else {
        let mut args = vec![];
        while let Some(arg) = argv_iter.next_if(|s| !s.starts_with("--")) {
            args.push(arg);
        }
        args
    }
}

#[cfg(test)]
mod test {
    use super::*;

    fn args(args: &[&str]) -> Vec<String> {
        ["foo"].iter().chain(args).map(|&s| s.to_owned()).collect()
    }

    #[test]
    fn test_flag_help() {
        let mut bar = None;
        let mut parser = Flags::new();
        parser.define_flag("--bar", "bar help", &mut bar);
        let result = parser.parse(args(&["--help"])).unwrap();
        if let ParseOutcome::Help(h) = result {
            assert!(h.contains("Help for foo"));
            assert!(h.contains("--bar\tbar help"));
        } else {
            panic!("expected that --help would invoke help, instead parsed arguments")
        }
    }

    #[test]
    fn test_flag_single_repeated() {
        let mut bar = None;
        let mut parser = Flags::new();
        parser.define_flag("--bar", "bar help", &mut bar);
        let result = parser.parse(args(&["--bar", "aa", "bb"]));
        if let Err(FlagParseError::ProvidedMultipleTimes(f)) = result {
            assert_eq!(f, "--bar");
        } else {
            panic!("expected error, got {:?}", result)
        }
        let mut parser = Flags::new();
        parser.define_flag("--bar", "bar help", &mut bar);
        let result = parser.parse(args(&["--bar", "aa", "--bar", "bb"]));
        if let Err(FlagParseError::ProvidedMultipleTimes(f)) = result {
            assert_eq!(f, "--bar");
        } else {
            panic!("expected error, got {:?}", result)
        }
    }

    #[test]
    fn test_repeated_flags() {
        // Test case 1) --bar something something_else should work as a repeated flag.
        let mut bar = None;
        let mut parser = Flags::new();
        parser.define_repeated_flag("--bar", "bar help", &mut bar);
        let result = parser.parse(args(&["--bar", "aa", "bb"])).unwrap();
        assert!(matches!(result, ParseOutcome::Parsed(_)));
        assert_eq!(bar, Some(vec!["aa".to_owned(), "bb".to_owned()]));
        // Test case 2) --bar something --bar something_else should also work as a repeated flag.
        bar = None;
        let mut parser = Flags::new();
        parser.define_repeated_flag("--bar", "bar help", &mut bar);
        let result = parser.parse(args(&["--bar", "aa", "--bar", "bb"])).unwrap();
        assert!(matches!(result, ParseOutcome::Parsed(_)));
        assert_eq!(bar, Some(vec!["aa".to_owned(), "bb".to_owned()]));
    }

    #[test]
    fn test_extra_args() {
        let parser = Flags::new();
        let result = parser.parse(args(&["--", "bb"])).unwrap();
        if let ParseOutcome::Parsed(got) = result {
            assert_eq!(got, vec!["bb".to_owned()])
        } else {
            panic!("expected correct parsing, got {:?}", result)
        }
    }
}
