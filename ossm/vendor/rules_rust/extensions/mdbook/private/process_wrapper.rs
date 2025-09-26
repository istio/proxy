//! The `rules_mdbook` process wrapper

use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::{env, fs};

#[cfg(target_family = "unix")]
const PATH_SEP: &str = ":";

#[cfg(target_family = "windows")]
const PATH_SEP: &str = ";";

struct Args {
    pub inputs_manifest: BTreeMap<PathBuf, PathBuf>,

    pub output: PathBuf,

    pub mdbook: PathBuf,

    pub mdbook_args: Vec<String>,
}

impl Args {
    pub fn parse() -> Self {
        let mut args = env::args();

        // Skip argv0 (process wrapper path)
        args.next();

        let inputs_manifest = PathBuf::from(
            args.next()
                .expect("Not enough args passed to process-wrapper."),
        );

        let inputs_manifest = action_args::try_parse_args(&inputs_manifest)
            .unwrap()
            .into_iter()
            .map(|arg| {
                let (path, short_path) = arg
                    .split_once("=")
                    .unwrap_or_else(|| panic!("Failed to split input manifest arg `{}`", arg));
                (PathBuf::from(path), PathBuf::from(short_path))
            })
            .collect();

        let output = PathBuf::from(
            args.next()
                .expect("Not enough args passed to process-wrapper."),
        );

        let mdbook = PathBuf::from(
            args.next()
                .expect("Not enough args passed to process-wrapper."),
        );

        let mdbook_args = args.collect::<Vec<String>>();

        Self {
            output,
            inputs_manifest,
            mdbook,
            mdbook_args,
        }
    }
}

fn generate_work_dir(output_dir: &Path, inputs_manifest: &BTreeMap<PathBuf, PathBuf>) -> PathBuf {
    let workdir = output_dir.parent().unwrap().join(format!(
        "{}_",
        output_dir.file_name().unwrap().to_string_lossy()
    ));

    for (src, dest) in inputs_manifest.iter() {
        let abs_dest = workdir.join(dest);
        fs::create_dir_all(abs_dest.parent().unwrap()).unwrap();
        fs::copy(src, &abs_dest).unwrap_or_else(|e| {
            panic!(
                "Failed to copy `{} -> {}`\n{}",
                src.display(),
                abs_dest.display(),
                e
            )
        });
    }

    workdir
}

fn main() {
    let args = Args::parse();

    let pwd = env::current_dir().expect("Unable to determine current working directory");

    let mut command = Command::new(&args.mdbook);

    // Inject plugin paths into PATH
    if let Ok(plugin_path) = env::var("MDBOOK_PLUGIN_PATH") {
        if !plugin_path.is_empty() {
            let path = env::var("PATH").unwrap_or_default();
            let plugin_path = plugin_path.replace("${pwd}", &pwd.to_string_lossy());
            command.env("PATH", format!("{}{}{}", plugin_path, PATH_SEP, path));
        }
    }

    // Flatten all inputs to a runfiles-like directory so generated files correctly appear
    // relative to other source files.
    let work_dir = generate_work_dir(&pwd.join(&args.output), &args.inputs_manifest);

    // Run mdbook and save output
    command
        .args(
            args.mdbook_args
                .iter()
                .map(|arg| arg.replace("${pwd}", &work_dir.to_string_lossy())),
        )
        .arg("--dest-dir")
        .arg(pwd.join(&args.output));

    let output = command
        .output()
        .unwrap_or_else(|e| panic!("Failed to spawn mdbook command\n{:?}\n{:#?}", e, command));

    fs::remove_dir_all(&work_dir).unwrap();

    if !output.status.success() {
        eprintln!("{}", String::from_utf8(output.stdout).unwrap());
        eprintln!("{}", String::from_utf8(output.stderr).unwrap());
        std::process::exit(output.status.code().unwrap_or(1));
    }
}
