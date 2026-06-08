//! A tool for writing stubs from a [`pyo3::PyModule`].

use std::env;
use std::fs;
use std::path::PathBuf;

use pyo3_introspection::{introspect_cdylib, module_stub_files};

#[derive(Debug)]
struct Args {
    /// The name of the PyO3 module.
    module_name: String,

    /// The path of the PyO3 library file.
    module_path: PathBuf,

    /// The output path for the stubs file
    output: PathBuf,
}

impl Args {
    pub fn parse() -> Self {
        let mut module_name = None;
        let mut module_path = None;
        let mut output = None;

        for arg in env::args().skip(1) {
            if let Some((key, value)) = arg.split_once('=') {
                match key {
                    "--module_name" => module_name = Some(value.to_string()),
                    "--module_path" => module_path = Some(PathBuf::from(value)),
                    "--output" => output = Some(PathBuf::from(value)),
                    _ => panic!("Unknown argument: {}", key),
                }
            } else {
                panic!("Invalid argument format: {}", arg);
            }
        }

        Self {
            module_name: module_name.expect("Missing --module_name argument"),
            module_path: module_path.expect("Missing --module_path argument"),
            output: output.expect("Missing --output argument"),
        }
    }
}

fn main() {
    let args = Args::parse();

    // Load the module
    let module = introspect_cdylib(&args.module_path, &args.module_name)
        .expect("Failed to parse stubs from module.");

    // Generate stubs
    let mut actual_stubs = module_stub_files(&module);

    // Extract stubs for root module.
    let content = &actual_stubs
        .remove(&PathBuf::from("__init__.pyi"))
        .expect("Failed to locate stubs for root module.");

    if !actual_stubs.is_empty() {
        eprintln!("WARNING: Dropped stubs: {:#?}", actual_stubs);
    }

    // Save results
    if let Some(parent) = args.output.parent() {
        fs::create_dir_all(parent).unwrap();
    }
    fs::write(&args.output, content).unwrap();
}
