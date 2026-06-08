use std::ffi::OsString;
use std::path::PathBuf;
use std::process::{exit, Command};

const WRAPPED_TOOL_NAME: &str = env!("WRAPPED_TOOL_NAME");
const WRAPPED_TOOL_TARGET: &str = env!("WRAPPED_TOOL_TARGET");
const WRAPPED_TOOL_ROOTPATH: &str = env!("WRAPPED_TOOL_ROOTPATH");

#[cfg(not(target_os = "windows"))]
const PATH_SEPARATOR: &str = ":";
#[cfg(target_os = "windows")]
const PATH_SEPARATOR: &str = ";";

fn main() {
    let wrapped_tool_path: PathBuf = runfiles::Runfiles::create()
        .and_then(|runfiles| {
            let path = runfiles::rlocation!(runfiles, WRAPPED_TOOL_TARGET).unwrap();
            if !path.exists() {
                return Err(runfiles::RunfilesError::RunfileNotFound(path));
            }
            Ok(path)
        })
        .unwrap_or_else(|_| {
            std::env::current_dir()
                .expect("Failed to get current directory")
                .join(WRAPPED_TOOL_ROOTPATH)
        });

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
        .unwrap_or_else(|| std::env::current_dir().expect("Failed to get build working directory"));

    let mut command = Command::new(wrapped_tool_path);
    command
        .current_dir(&working_directory)
        .args(std::env::args_os().skip(1))
        .env("PATH", new_path);
    let status = command
        .status()
        .unwrap_or_else(|e| panic!("Failed to run spawn command: {:#?}\n{:#}", command, e));
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
