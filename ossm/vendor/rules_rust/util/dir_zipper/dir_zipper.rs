use std::ffi::OsString;
use std::path::PathBuf;
use std::process::Command;

const USAGE: &str = r#"usage: dir_zipper <zipper> <output> <root-dir> [<file>...]

Creates a zip archive, stripping a directory prefix from each file name.

Args:
  zipper: Path to @bazel_tools//tools/zip:zipper.
  output: Path to zip file to create: e.g., "/tmp/out.zip".
  root_dir: Directory to strip from each archive name, with no trailing
    slash: e.g., "/tmp/myfiles".
  files: List of files to include in the archive, all under `root_dir`:
    e.g., ["/tmp/myfiles/a", "/tmp/myfiles/b/c"].

Example:
  dir_zipper \
    bazel-rules_rust/external/bazel_tools/tools/zip/zipper/zipper \
    /tmp/out.zip \
    /tmp/myfiles \
    /tmp/myfiles/a /tmp/myfiles/b/c

This will create /tmp/out.zip with file entries "a" and "b/c".
"#;

macro_rules! die {
    ($($arg:tt)*) => {
        {
            eprintln!($($arg)*);
            std::process::exit(1);
        }
    };
}

fn main() {
    let mut args = std::env::args_os().skip(1);
    let (zipper, output, root_dir) = match args.next().zip(args.next()).zip(args.next()) {
        Some(((zipper, output), root_dir)) => (
            PathBuf::from(zipper),
            PathBuf::from(output),
            PathBuf::from(root_dir),
        ),
        _ => {
            die!("{}", USAGE);
        }
    };
    let files = args.map(PathBuf::from).collect::<Vec<_>>();
    let mut comm = Command::new(zipper);
    comm.arg("c"); // create, but don't compress
    comm.arg(output);
    for f in files {
        let rel = f.strip_prefix(&root_dir).unwrap_or_else(|_e| {
            die!(
                "fatal: non-descendant: {} not under {}",
                f.display(),
                root_dir.display()
            );
        });
        let mut spec = OsString::new();
        spec.push(rel);
        spec.push("=");
        spec.push(f);
        comm.arg(spec);
    }
    let exit_status = comm
        .spawn()
        .unwrap_or_else(|e| die!("fatal: could not spawn zipper: {}", e))
        .wait()
        .unwrap_or_else(|e| die!("fatal: could not wait on zipper: {}", e));
    if !exit_status.success() {
        match exit_status.code() {
            Some(c) => std::process::exit(c),
            None => die!("fatal: zipper terminated by signal"),
        }
    }
}
