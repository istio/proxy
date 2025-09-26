use std::ffi::OsString;
use std::path::PathBuf;
use std::process::{exit, Command};

const WRAPPED_TOOL_NAME: &str = env!("WRAPPED_TOOL_NAME");
const WRAPPED_TOOL_TARGET: &str = env!("WRAPPED_TOOL_TARGET");

#[cfg(not(target_os = "windows"))]
const PATH_SEPARATOR: &str = ":";
#[cfg(target_os = "windows")]
const PATH_SEPARATOR: &str = ";";

fn main() {
    let runfiles = runfiles::Runfiles::create().unwrap();

    let wrapped_tool_path = runfiles::rlocation!(runfiles, WRAPPED_TOOL_TARGET).unwrap();
    if !wrapped_tool_path.exists() {
        panic!(
            "{WRAPPED_TOOL_NAME} does not exist at: {}",
            wrapped_tool_path.display()
        );
    }

    let tool_directory = wrapped_tool_path
        .parent()
        .expect("parent directory of tool binary");
    let old_path = std::env::var_os("PATH").unwrap_or_default();
    let mut new_path = OsString::from(tool_directory);
    new_path.push(PATH_SEPARATOR);
    new_path.push(&old_path);

    let working_directory = std::env::var_os("BUILD_WORKING_DIRECTORY")
        .map(PathBuf::from)
        .unwrap_or_else(|| std::env::current_dir().expect("Failed to get working directory"));

    let status = Command::new(wrapped_tool_path)
        .current_dir(&working_directory)
        .args(std::env::args_os().skip(1))
        .env("PATH", new_path)
        .status()
        .unwrap_or_else(|e| panic!("Failed to run {WRAPPED_TOOL_NAME} {:#}", e));
    if let Some(exit_code) = status.code() {
        exit(exit_code);
    }
    exit_for_signal(&status);
    panic!("Child rustfmt process didn't exit or get a signal - don't know how to handle it");
}

#[cfg(target_family = "unix")]
fn exit_for_signal(status: &std::process::ExitStatus) {
    use std::os::unix::process::ExitStatusExt;
    if let Some(signal) = status.signal() {
        exit(signal);
    }
}

#[cfg(not(target_family = "unix"))]
#[allow(unused)]
fn exit_for_signal(status: &std::process::ExitStatus) {}
