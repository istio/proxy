//! This script collects code coverage data for Rust sources, after the tests
//! were executed.
//!
//! By taking advantage of Bazel C++ code coverage collection, this script is
//! able to be executed by the existing coverage collection mechanics.
//!
//! Bazel uses the lcov tool for gathering coverage data. There is also
//! an experimental support for clang llvm coverage, which uses the .profraw
//! data files to compute the coverage report.
//!
//! This script assumes the following environment variables are set:
//! - COVERAGE_DIR            Directory containing metadata files needed for
//!                           coverage collection (e.g. gcda files, profraw).
//! - COVERAGE_OUTPUT_FILE    The coverage action output path.
//! - ROOT                    Location from where the code coverage collection
//!                           was invoked.
//! - RUNFILES_DIR            Location of the test's runfiles.
//! - VERBOSE_COVERAGE        Print debug info from the coverage scripts
//!
//! The script looks in $COVERAGE_DIR for the Rust metadata coverage files
//! (profraw) and uses lcov to get the coverage data. The coverage data
//! is placed in $COVERAGE_DIR as a `coverage.dat` file.

use std::env;
use std::fs;
use std::path::Path;
use std::path::PathBuf;
use std::process;

macro_rules! log {
    ($($arg:tt)*) => {
        if env::var("VERBOSE_COVERAGE").is_ok() {
            eprintln!($($arg)*);
        }
    };
}

fn find_metadata_file(execroot: &Path, runfiles_dir: &Path, path: &str) -> PathBuf {
    if execroot.join(path).exists() {
        return execroot.join(path);
    }

    log!(
        "File does not exist in execroot, falling back to runfiles: {}",
        path
    );

    runfiles_dir.join(path)
}

fn find_test_binary(execroot: &Path, runfiles_dir: &Path) -> PathBuf {
    let test_binary = runfiles_dir
        .join(env::var("TEST_WORKSPACE").unwrap())
        .join(env::var("TEST_BINARY").unwrap());

    if !test_binary.exists() {
        let configuration = runfiles_dir
            .strip_prefix(execroot)
            .expect("RUNFILES_DIR should be relative to ROOT")
            .components()
            .enumerate()
            .filter_map(|(i, part)| {
                // Keep only `bazel-out/<configuration>/bin`
                if i < 3 {
                    Some(PathBuf::from(part.as_os_str()))
                } else {
                    None
                }
            })
            .fold(PathBuf::new(), |mut path, part| {
                path.push(part);
                path
            });

        let test_binary = execroot
            .join(configuration)
            .join(env::var("TEST_BINARY").unwrap());

        log!(
            "TEST_BINARY is not found in runfiles. Falling back to: {}",
            test_binary.display()
        );

        test_binary
    } else {
        test_binary
    }
}

fn main() {
    let coverage_dir = PathBuf::from(env::var("COVERAGE_DIR").unwrap());
    let execroot = PathBuf::from(env::var("ROOT").unwrap());
    let mut runfiles_dir = PathBuf::from(env::var("RUNFILES_DIR").unwrap());

    if !runfiles_dir.is_absolute() {
        runfiles_dir = execroot.join(runfiles_dir);
    }

    log!("ROOT: {}", execroot.display());
    log!("RUNFILES_DIR: {}", runfiles_dir.display());

    let coverage_output_file = coverage_dir.join("coverage.dat");
    let profdata_file = coverage_dir.join("coverage.profdata");
    let llvm_cov = find_metadata_file(
        &execroot,
        &runfiles_dir,
        &env::var("RUST_LLVM_COV").unwrap(),
    );
    let llvm_profdata = find_metadata_file(
        &execroot,
        &runfiles_dir,
        &env::var("RUST_LLVM_PROFDATA").unwrap(),
    );
    let test_binary = find_test_binary(&execroot, &runfiles_dir);
    let profraw_files: Vec<PathBuf> = fs::read_dir(coverage_dir)
        .unwrap()
        .flatten()
        .filter_map(|entry| {
            let path = entry.path();
            if let Some(ext) = path.extension() {
                if ext == "profraw" {
                    return Some(path);
                }
            }
            None
        })
        .collect();

    let mut llvm_profdata_cmd = process::Command::new(llvm_profdata);
    llvm_profdata_cmd
        .arg("merge")
        .arg("--sparse")
        .args(profraw_files)
        .arg("--output")
        .arg(&profdata_file);

    log!("Spawning {:#?}", llvm_profdata_cmd);
    let status = llvm_profdata_cmd
        .status()
        .expect("Failed to spawn llvm-profdata process");

    if !status.success() {
        process::exit(status.code().unwrap_or(1));
    }

    let mut llvm_cov_cmd = process::Command::new(llvm_cov);
    llvm_cov_cmd
        .arg("export")
        .arg("-format=lcov")
        .arg("-instr-profile")
        .arg(&profdata_file)
        .arg("-ignore-filename-regex='.*external/.+'")
        .arg("-ignore-filename-regex='/tmp/.+'")
        .arg(format!("-path-equivalence=.,'{}'", execroot.display()))
        .arg(test_binary)
        .stdout(process::Stdio::piped());

    log!("Spawning {:#?}", llvm_cov_cmd);
    let child = llvm_cov_cmd
        .spawn()
        .expect("Failed to spawn llvm-cov process");

    let output = child.wait_with_output().expect("llvm-cov process failed");

    // Parse the child process's stdout to a string now that it's complete.
    log!("Parsing llvm-cov output");
    let report_str = std::str::from_utf8(&output.stdout).expect("Failed to parse llvm-cov output");

    log!("Writing output to {}", coverage_output_file.display());
    fs::write(
        coverage_output_file,
        report_str
            .replace("#/proc/self/cwd/", "")
            .replace(&execroot.display().to_string(), ""),
    )
    .unwrap();

    // Destroy the intermediate binary file so lcov_merger doesn't parse it twice.
    log!("Cleaning up {}", profdata_file.display());
    fs::remove_file(profdata_file).unwrap();

    log!("Success!");
}
