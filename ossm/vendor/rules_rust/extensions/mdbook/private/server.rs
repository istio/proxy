//! A process wrapper for `mdbook serve`.

use std::path::PathBuf;
use std::process::Command;
use std::{env, fs};

use runfiles::rlocation;

#[cfg(target_family = "unix")]
const PATH_SEP: &str = ":";

#[cfg(target_family = "windows")]
const PATH_SEP: &str = ";";

struct Args {
    pub mdbook: PathBuf,

    pub config: PathBuf,

    pub hostname: String,

    pub port: String,

    pub plugins: Vec<PathBuf>,

    pub mdbook_args: Vec<String>,
}

impl Args {
    pub fn parse() -> Self {
        let runfiles = runfiles::Runfiles::create().unwrap();

        let args_env = env::var("RULES_MDBOOK_SERVE_ARGS_FILE").unwrap();
        let args_file = rlocation!(runfiles, args_env).unwrap();
        let raw_args = action_args::try_parse_args(&args_file).unwrap();

        let mut mdbook: Option<PathBuf> = None;
        let mut config: Option<PathBuf> = None;
        let mut hostname: Option<String> = None;
        let mut port: Option<String> = None;
        let mut plugins: Vec<PathBuf> = Vec::new();

        for arg in raw_args {
            if arg.starts_with("--mdbook=") {
                let val = arg.split_once("=").unwrap().1;
                mdbook = Some(rlocation!(runfiles, val).unwrap());
            } else if arg.starts_with("--plugin=") {
                let val = arg.split_once("=").unwrap().1.to_string();
                plugins.push(rlocation!(runfiles, val).unwrap());
            } else if arg.starts_with("--config=") {
                let val = arg.split_once("=").unwrap().1.to_string();
                config = Some(rlocation!(runfiles, val).unwrap());
            } else if arg.starts_with("--hostname=") {
                hostname = Some(arg.split_once("=").unwrap().1.to_string());
            } else if arg.starts_with("--port=") {
                port = Some(arg.split_once("=").unwrap().1.to_string());
            }
        }

        Self {
            mdbook: mdbook.unwrap(),
            config: config.unwrap(),
            hostname: hostname.unwrap(),
            port: port.unwrap(),
            plugins,
            mdbook_args: env::args().skip(1).collect(),
        }
    }
}

const RULES_MDBOOK_TMP_NAME: &str = "rules_mdbook_server";

fn make_temp_dir() -> PathBuf {
    if let Ok(var) = env::var("TMP") {
        return PathBuf::from(var).join(RULES_MDBOOK_TMP_NAME);
    }

    if let Ok(var) = env::var("TEMP") {
        return PathBuf::from(var).join(RULES_MDBOOK_TMP_NAME);
    }

    if let Ok(var) = env::var("TMPDIR") {
        return PathBuf::from(var).join(RULES_MDBOOK_TMP_NAME);
    }

    if let Ok(var) = env::var("TEMPDIR") {
        return PathBuf::from(var).join(RULES_MDBOOK_TMP_NAME);
    }

    let tmp = PathBuf::from("/tmp");
    if tmp.exists() {
        return tmp.join(RULES_MDBOOK_TMP_NAME);
    }

    if let Ok(var) = env::var("USERPROFILE") {
        let tmp = PathBuf::from(var)
            .join("AppData")
            .join("Local")
            .join("Temp");
        if tmp.exists() {
            return tmp.join(RULES_MDBOOK_TMP_NAME);
        }
    }

    panic!("Could not determine how to create temp dir.")
}

fn main() {
    let args = Args::parse();

    let mut command = Command::new(&args.mdbook);

    // Inject plugin paths into PATH
    let pwd = env::current_dir().expect("Unable to determine current working directory");
    if !args.plugins.is_empty() {
        let path = env::var("PATH").unwrap_or_default();

        let plugin_path = args
            .plugins
            .iter()
            .map(|p| {
                if p.is_absolute() {
                    p.parent().unwrap().to_string_lossy().to_string()
                } else {
                    let abs = pwd.join(p);
                    abs.parent().unwrap().to_string_lossy().to_string()
                }
            })
            .collect::<Vec<_>>()
            .join(PATH_SEP);

        command.env("PATH", format!("{}{}{}", plugin_path, PATH_SEP, path));
    }

    command
        .arg("serve")
        .arg(args.config.parent().unwrap())
        .args(&args.mdbook_args);

    // Add default hostname value if commandline was not specified.
    if !args.mdbook_args.iter().any(|arg| {
        ["-n", "--hostname"].contains(&arg.as_str())
            || arg.starts_with("-n=")
            || arg.starts_with("--hostname=")
    }) {
        command.args(["--hostname", &args.hostname]);
    }

    // Add default port value if commandline was not specified.
    if !args.mdbook_args.iter().any(|arg| {
        ["-p", "--port"].contains(&arg.as_str())
            || arg.starts_with("-p=")
            || arg.starts_with("--port=")
    }) {
        command.args(["--port", &args.port]);
    }

    // Check if `-d` or `--dest-dir` was passed. If not, make a temp dir
    let temp_dir: Option<PathBuf> = if !args.mdbook_args.iter().any(|a| {
        ["-d", "--dest-dir"].contains(&a.as_str())
            || a.starts_with("-d=")
            || a.starts_with("--dest-dir=")
    }) {
        let temp_dir = make_temp_dir();
        command.arg("--dest-dir").arg(&temp_dir);
        Some(temp_dir)
    } else {
        None
    };

    // Run mdbook and save output
    let status = command
        .status()
        .unwrap_or_else(|e| panic!("Failed to spawn mdbook command\n{:?}\n{:#?}", e, command));

    if let Some(path) = temp_dir {
        fs::remove_dir_all(&path).unwrap();
    }

    if !status.success() {
        std::process::exit(status.code().unwrap_or(1));
    }
}
