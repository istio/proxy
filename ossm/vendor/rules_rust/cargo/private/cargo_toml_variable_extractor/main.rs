use std::{
    borrow::Cow,
    env::args_os,
    fs::{read_to_string, File},
    io::Write,
    path::{Path, PathBuf},
};

use cargo_util_schemas::manifest::{
    InheritableField, InheritablePackage, InheritableString, InheritableStringOrBool, StringOrBool,
    TomlManifest, TomlPackage,
};

fn main() {
    let args: Vec<_> = args_os().collect();
    let (out_path, manifest, workspace) = match &args[..] {
        [_, out_path, manifest_path] => (out_path, parse_manifest(manifest_path), None),
        [_, out_path, manifest_path, workspace_manifest_path] => (
            out_path,
            parse_manifest(manifest_path),
            parse_manifest(workspace_manifest_path)
                .workspace
                .and_then(|ws| ws.package)
                .map(|ws| {
                    let path_relative_to_crate = pathdiff::diff_paths(
                        Path::new(workspace_manifest_path).parent().unwrap(),
                        Path::new(manifest_path).parent().unwrap(),
                    )
                    .expect("Couldn't get path of workspace relative to crate");
                    Workspace {
                        manifest: ws,
                        path_relative_to_crate,
                    }
                }),
        ),
        other => {
            let argv0 = other.first().map_or_else(
                || Cow::Borrowed("cargo-toml-parser"),
                |v| v.to_string_lossy(),
            );
            panic!(
                "Usage: {} path/to/Cargo.toml [path/to/workspace/Cargo.toml]",
                argv0
            );
        }
    };

    let mut out = File::create(out_path).expect("Failed to create output file");
    print_manifest_env_vars(&mut out, &manifest, workspace.as_ref());
}

#[derive(Debug)]
struct Workspace {
    manifest: InheritablePackage,
    path_relative_to_crate: PathBuf,
}

macro_rules! print_inheritable_str {
    ($out:expr, $name:expr, $manifest:expr, $workspace_manifest:expr, $closure:expr $(,)?) => {
        print_inheritable_str(
            $out,
            $name,
            $manifest,
            $workspace_manifest,
            $closure,
            $closure,
        )
    };
}

macro_rules! get_inheritable_value {
    ($manifest:expr, $workspace_manifest:expr, $closure:expr $(,)?) => {
        get_inheritable_value($manifest, $workspace_manifest, $closure, $closure)
    };
}

fn print_manifest_env_vars(
    out: &mut impl Write,
    manifest: &TomlManifest,
    workspace: Option<&Workspace>,
) {
    let workspace_manifest = &workspace.as_ref().map(|w| &w.manifest);
    let default_version =
        semver::Version::parse("0.0.0").expect("Known-good version 0.0.0 couldn't be parsed");

    let version = if let Some(version) =
        get_inheritable_value!(manifest, workspace_manifest, |p| p.version.as_ref())
    {
        version
    } else {
        &default_version
    };
    print_env_str(out, "CARGO_PKG_VERSION", &version.to_string());
    print_env(out, "CARGO_PKG_VERSION_MAJOR", version.major);
    print_env(out, "CARGO_PKG_VERSION_MINOR", version.minor);
    print_env(out, "CARGO_PKG_VERSION_PATCH", version.patch);
    print_env_str(out, "CARGO_PKG_VERSION_PRE", version.pre.as_str());
    print_optional_env_str(
        out,
        "CARGO_PKG_NAME",
        manifest.package.as_ref().map(|p| p.name.as_str()),
    );
    let maybe_authors =
        get_inheritable_value!(manifest, workspace_manifest, |p| p.authors.as_ref());
    let authors = maybe_authors.map_or_else(String::new, |authors| authors.join(":"));
    print_env_str(out, "CARGO_PKG_AUTHORS", &authors);
    print_inheritable_str!(
        out,
        "CARGO_PKG_DESCRIPTION",
        manifest,
        workspace_manifest,
        |p| p.description.as_ref()
    );
    print_inheritable_str!(
        out,
        "CARGO_PKG_HOMEPAGE",
        manifest,
        workspace_manifest,
        |p| p.homepage.as_ref(),
    );
    print_inheritable_str!(
        out,
        "CARGO_PKG_REPOSITORY",
        manifest,
        workspace_manifest,
        |p| p.repository.as_ref(),
    );
    print_inheritable_str!(
        out,
        "CARGO_PKG_LICENSE",
        manifest,
        workspace_manifest,
        |p| p.license.as_ref(),
    );
    print_inheritable_path(
        out,
        "CARGO_PKG_LICENSE_FILE",
        manifest
            .package
            .as_ref()
            .and_then(|p| p.license_file.clone()),
        workspace_manifest.and_then(|m| m.license_file.clone()),
        workspace.map(|w| w.path_relative_to_crate.as_path()),
    );
    if let Some(rust_version) =
        get_inheritable_value!(manifest, workspace_manifest, |p| p.rust_version.as_ref(),)
    {
        let rust_version = format!("{}", rust_version);
        print_env_str(out, "CARGO_PKG_RUST_VERSION", &rust_version);
    } else {
        print_env_str(out, "CARGO_PKG_RUST_VERSION", "");
    }
    let manifest_readme = manifest
        .package
        .as_ref()
        .and_then(|p| p.readme.clone())
        .and_then(|r| match r {
            InheritableStringOrBool::Value(StringOrBool::String(v)) => {
                Some(InheritableString::Value(v))
            }
            InheritableStringOrBool::Value(StringOrBool::Bool(true)) => {
                Some(InheritableString::Value("README.md".to_owned()))
            }
            InheritableStringOrBool::Value(StringOrBool::Bool(false)) => None,
            InheritableStringOrBool::Inherit(f) => Some(InheritableString::Inherit(f)),
        });
    let workspace_readme = match workspace_manifest.and_then(|m| m.readme.as_ref()) {
        Some(StringOrBool::String(v)) => Some(v.clone()),
        Some(StringOrBool::Bool(true)) => Some("README.md".to_owned()),
        Some(StringOrBool::Bool(false)) => None,
        None => None,
    };
    print_inheritable_path(
        out,
        "CARGO_PKG_README",
        manifest_readme,
        workspace_readme,
        workspace.map(|w| w.path_relative_to_crate.as_path()),
    );
}

fn parse_manifest<P: AsRef<Path>>(path: &P) -> TomlManifest {
    let path = path.as_ref();
    let content = read_to_string(path)
        .unwrap_or_else(|err| panic!("Failed to read {}: {}", path.display(), err));
    toml::from_str(&content)
        .unwrap_or_else(|err| panic!("Failed to parse {}: {}", path.display(), err))
}

fn print_inheritable_str<
    'a,
    S: AsRef<str> + 'a,
    F1: Fn(&'a TomlPackage) -> Option<&'a InheritableField<S>>,
    F2: Fn(&'a InheritablePackage) -> Option<&'a S>,
>(
    out: &mut impl Write,
    key: &str,
    manifest: &'a TomlManifest,
    workspace_manifest: &'a Option<&'a InheritablePackage>,
    get_manifest: F1,
    get_workspace: F2,
) {
    if let Some(value) =
        get_inheritable_value(manifest, workspace_manifest, get_manifest, get_workspace)
    {
        print_env_str(out, key, value.as_ref())
    } else {
        print_env_str(out, key, "");
    }
}

fn print_inheritable_path(
    out: &mut impl Write,
    key: &str,
    manifest_value: Option<InheritableString>,
    workspace_value: Option<String>,
    workspace_path_relative_to_crate: Option<&Path>,
) {
    let maybe_path = match (
        manifest_value,
        workspace_path_relative_to_crate.and_then(|workspace_path_relative_to_crate| {
            workspace_value
                .map(|workspace_value| (workspace_path_relative_to_crate, workspace_value))
        }),
    ) {
        (Some(InheritableString::Value(path)), _) => Some(path.to_owned()),
        (Some(InheritableString::Inherit(_)), Some((relpath, value_path))) => {
            let path = relpath.join(value_path);
            let joined = path
                .components()
                .map(|c| c.as_os_str().to_string_lossy().into_owned())
                .collect::<Vec<_>>()
                .join(std::path::MAIN_SEPARATOR_STR);
            Some(joined)
        }
        (Some(InheritableString::Inherit(_)), None) => {
            panic!("Can't inherit a {key} which was missing from the workspace")
        }
        _ => None,
    };
    print_optional_env_str(out, key, maybe_path.as_deref());
}

fn get_inheritable_value<
    'a,
    T,
    F1: Fn(&'a TomlPackage) -> Option<&'a InheritableField<T>>,
    F2: Fn(&'a InheritablePackage) -> Option<&'a T>,
>(
    manifest: &'a TomlManifest,
    workspace_manifest: &'a Option<&'a InheritablePackage>,
    get_manifest: F1,
    get_workspace: F2,
) -> Option<&'a T> {
    match (
        manifest.package.as_ref().and_then(|p| get_manifest(p)),
        workspace_manifest.and_then(get_workspace),
    ) {
        (Some(InheritableField::Value(value)), _) => Some(value),
        (Some(InheritableField::Inherit(..)), Some(value)) => Some(value),
        _ => None,
    }
}

fn print_optional_env_str(out: &mut impl Write, key: &str, value: Option<&str>) {
    if let Some(value) = value {
        print_env_str(out, key, value);
    } else {
        print_env_str(out, key, "");
    }
}

fn print_env_str(out: &mut impl Write, key: &str, value: &str) {
    writeln!(out, "{}={}", key, value.replace('\n', "\\\n")).expect("Failed to print env var");
}

fn print_env(out: &mut impl Write, key: &str, value: u64) {
    writeln!(out, "{}={}", key, value).expect("Failed to print env var");
}
