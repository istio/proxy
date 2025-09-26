//! A process wrapper for running a Protobuf compiler configured for Prost or Tonic output in a Bazel rule.

use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::fmt::{Display, Formatter, Write};
use std::fs;
use std::io::BufRead;
use std::path::Path;
use std::path::PathBuf;
use std::process;
use std::{env, fmt};

use heck::{ToSnakeCase, ToUpperCamelCase};
use prost::Message;
use prost_types::{
    DescriptorProto, EnumDescriptorProto, FileDescriptorProto, FileDescriptorSet,
    OneofDescriptorProto,
};

/// Locate prost outputs in the protoc output directory.
fn find_generated_rust_files(out_dir: &Path) -> BTreeSet<PathBuf> {
    let mut all_rs_files: BTreeSet<PathBuf> = BTreeSet::new();
    for entry in fs::read_dir(out_dir).expect("Failed to read directory") {
        let entry = entry.expect("Failed to read entry");
        let path = entry.path();
        if path.is_dir() {
            for f in find_generated_rust_files(&path) {
                all_rs_files.insert(f);
            }
        } else if let Some(ext) = path.extension() {
            if ext == "rs" {
                all_rs_files.insert(path);
            }
        } else if let Some(name) = path.file_name() {
            // The filename is set to `_` when the package name is empty.
            if name == "_" {
                let rs_name = path.parent().expect("Failed to get parent").join("_.rs");
                fs::rename(&path, &rs_name).unwrap_or_else(|err| {
                    panic!("Failed to rename file: {err:?}: {path:?} -> {rs_name:?}")
                });
                all_rs_files.insert(rs_name);
            }
        }
    }

    all_rs_files
}

fn snake_cased_package_name(package: &str) -> String {
    if package == "_" {
        return package.to_owned();
    }

    package
        .split('.')
        .map(|s| s.to_snake_case())
        .collect::<Vec<_>>()
        .join(".")
}

/// Rust module definition.
#[derive(Debug, Default)]
struct Module {
    /// The name of the module.
    name: String,

    /// The contents of the module.
    contents: String,

    /// The names of any other modules which are submodules of this module.
    submodules: BTreeMap<String, Module>,
}

impl Module {
    fn insert(&mut self, module_name: String, contents: String) {
        let module_parts = module_name.split('.').collect::<Vec<_>>();

        self.insert_module(module_parts.as_slice(), contents);
    }

    fn insert_module(&mut self, module_parts: &[&str], contents: String) -> &mut Module {
        let current_name = module_parts[0].to_string();

        // Insert empty module if it doesn't exist.
        self.submodules
            .entry(current_name.clone())
            .or_insert_with(|| Module {
                name: current_name.clone(),
                contents: "".to_string(),
                submodules: BTreeMap::new(),
            });

        let current_module = self.submodules.get_mut(&current_name).unwrap();

        // If this is the last part (current module) then add the contents.
        if module_parts.len() == 1 {
            current_module.contents = contents;
            return current_module;
        }

        current_module.insert_module(&module_parts[1..], contents)
    }
}

const ADDITIONAL_CONTENT_HEADER: &str =
    "// A D D I T I O N A L   S O U R C E S ========================================";

/// Generate a lib.rs file with all prost/tonic outputs embeeded in modules which
/// mirror the proto packages. For the example proto file we would expect to see
/// the Rust output that follows it.
///
/// ```proto
/// syntax = "proto3";
/// package examples.prost.helloworld;
///
/// message HelloRequest {
///     // Request message contains the name to be greeted
///     string name = 1;
/// }
//
/// message HelloReply {
///     // Reply contains the greeting message
///     string message = 1;
/// }
/// ```
///
/// This is expected to render out to something like the following. Note that
/// formatting is not applied so indentation may be missing in the actual output.
///
/// ```ignore
/// pub mod examples {
///     pub mod prost {
///         pub mod helloworld {
///             // @generated
///             #[allow(clippy::derive_partial_eq_without_eq)]
///             #[derive(Clone, PartialEq, ::prost::Message)]
///             pub struct HelloRequest {
///                 /// Request message contains the name to be greeted
///                 #[prost(string, tag = "1")]
///                 pub name: ::prost::alloc::string::String,
///             }
///             #[allow(clippy::derive_partial_eq_without_eq)]
///             #[derive(Clone, PartialEq, ::prost::Message)]
///             pub struct HelloReply {
///                 /// Reply contains the greeting message
///                 #[prost(string, tag = "1")]
///                 pub message: ::prost::alloc::string::String,
///             }
///             // @protoc_insertion_point(module)
///         }
///     }
/// }
/// ```
fn generate_lib_rs(
    prost_outputs: &BTreeSet<PathBuf>,
    is_tonic: bool,
    direct_dep_crate_names: Vec<String>,
    additional_content: String,
) -> String {
    let mut contents = vec!["// @generated".to_string(), "".to_string()];
    for crate_name in direct_dep_crate_names {
        contents.push(format!("pub use {crate_name};"));
    }
    contents.push("".to_string());

    let mut module_info = Module {
        name: "".to_string(),
        contents: contents.join("\n"),
        submodules: BTreeMap::new(),
    };

    for path in prost_outputs.iter() {
        let mut package = path
            .file_stem()
            .expect("Failed to get file stem")
            .to_str()
            .expect("Failed to convert to str")
            .to_string();

        if is_tonic {
            package = package
                .strip_suffix(".tonic")
                .expect("Failed to strip suffix")
                .to_string()
        };

        if package.is_empty() {
            continue;
        }

        // Avoid a stack overflow by skipping a known bad package name
        let module_name = snake_cased_package_name(&package);

        let contents = fs::read_to_string(path).expect("Failed to read file");
        module_info.insert(module_name, contents);
    }

    let mut content = String::new();
    write_module(&mut content, &module_info, 0);

    if !additional_content.is_empty() {
        return format!(
            "{}\n\n{}\n\n{}",
            content, ADDITIONAL_CONTENT_HEADER, additional_content
        );
    }

    content
}

/// Write out a rust module and all of its submodules.
fn write_module(content: &mut String, module: &Module, depth: usize) {
    if module.name.is_empty() {
        content
            .write_str(&module.contents)
            .expect("Failed to write string");
        for submodule in module.submodules.values() {
            write_module(content, submodule, depth);
        }
        return;
    }
    let indent = "  ".repeat(depth);
    let is_rust_module = module.name != "_";

    if is_rust_module {
        let rust_module_name = escape_keyword(module.name.clone());
        content
            .write_str(&format!("{}pub mod {} {{\n", indent, rust_module_name))
            .expect("Failed to write string");
    }

    content
        .write_str(&module.contents)
        .expect("Failed to write string");

    for submodule in module.submodules.values() {
        write_module(content, submodule, depth + 1);
    }

    if is_rust_module {
        content
            .write_str(&format!("{}}}\n", indent))
            .expect("Failed to write string");
    }
}

/// ProtoPath is a path to a proto message, enum, or oneof.
///
/// Example: `helloworld.Greeter.HelloRequest`
#[derive(Debug, Clone, Ord, PartialOrd, Eq, PartialEq)]
struct ProtoPath(String);

impl ProtoPath {
    /// Join a component to the end of the path.
    fn join(&self, component: &str) -> ProtoPath {
        if self.0.is_empty() {
            return ProtoPath(component.to_string());
        }
        if component.is_empty() {
            return self.clone();
        }

        ProtoPath(format!("{}.{}", self.0, component))
    }
}

impl Display for ProtoPath {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<&str> for ProtoPath {
    fn from(path: &str) -> Self {
        ProtoPath(path.to_string())
    }
}

/// RustModulePath is a path to a rust module.
///
/// Example: `helloworld::greeter::HelloRequest`
#[derive(Debug, Clone, Ord, PartialOrd, Eq, PartialEq)]
struct RustModulePath(String);

impl RustModulePath {
    /// Join a path to the end of the module path.
    fn join(&self, path: &str) -> RustModulePath {
        if self.0.is_empty() {
            return RustModulePath(escape_keyword(path.to_string()));
        }
        if path.is_empty() {
            return self.clone();
        }

        RustModulePath(format!("{}::{}", self.0, escape_keyword(path.to_string())))
    }
}

impl Display for RustModulePath {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<&str> for RustModulePath {
    fn from(path: &str) -> Self {
        RustModulePath(escape_keyword(path.to_string()))
    }
}

/// Compute the `--extern_path` flags for a list of proto files. This is
/// expected to convert proto files into a BTreeMap of
/// `example.prost.helloworld`: `crate_name::example::prost::helloworld`.
fn get_extern_paths(
    descriptor_set: &FileDescriptorSet,
    crate_name: &str,
) -> Result<BTreeMap<ProtoPath, RustModulePath>, String> {
    let mut extern_paths = BTreeMap::new();
    let rust_path = RustModulePath(escape_keyword(crate_name.to_string()));

    for file in descriptor_set.file.iter() {
        descriptor_set_file_to_extern_paths(&mut extern_paths, &rust_path, file);
    }

    Ok(extern_paths)
}

/// Add the extern_path pairs for a file descriptor type.
fn descriptor_set_file_to_extern_paths(
    extern_paths: &mut BTreeMap<ProtoPath, RustModulePath>,
    rust_path: &RustModulePath,
    file: &FileDescriptorProto,
) {
    let package = file.package.clone().unwrap_or_default();
    let rust_path = package.split('.').fold(rust_path.clone(), |acc, part| {
        acc.join(&snake_cased_package_name(part))
    });
    let proto_path = ProtoPath(package);

    for message_type in file.message_type.iter() {
        message_type_to_extern_paths(extern_paths, &proto_path, &rust_path, message_type);
    }

    for enum_type in file.enum_type.iter() {
        enum_type_to_extern_paths(extern_paths, &proto_path, &rust_path, enum_type);
    }
}

/// Add the extern_path pairs for a message descriptor type.
fn message_type_to_extern_paths(
    extern_paths: &mut BTreeMap<ProtoPath, RustModulePath>,
    proto_path: &ProtoPath,
    rust_path: &RustModulePath,
    message_type: &DescriptorProto,
) {
    let message_type_name = message_type
        .name
        .as_ref()
        .expect("Failed to get message type name");

    extern_paths.insert(
        proto_path.join(message_type_name),
        rust_path.join(&message_type_name.to_upper_camel_case()),
    );

    let name_lower = message_type_name.to_lowercase();
    let proto_path = proto_path.join(&name_lower);
    let rust_path = rust_path.join(&name_lower);

    for nested_type in message_type.nested_type.iter() {
        message_type_to_extern_paths(extern_paths, &proto_path, &rust_path, nested_type)
    }

    for enum_type in message_type.enum_type.iter() {
        enum_type_to_extern_paths(extern_paths, &proto_path, &rust_path, enum_type);
    }

    for oneof_type in message_type.oneof_decl.iter() {
        oneof_type_to_extern_paths(extern_paths, &proto_path, &rust_path, oneof_type);
    }
}

/// Add the extern_path pairs for an enum type.
fn enum_type_to_extern_paths(
    extern_paths: &mut BTreeMap<ProtoPath, RustModulePath>,
    proto_path: &ProtoPath,
    rust_path: &RustModulePath,
    enum_type: &EnumDescriptorProto,
) {
    let enum_type_name = enum_type
        .name
        .as_ref()
        .expect("Failed to get enum type name");
    extern_paths.insert(
        proto_path.join(enum_type_name),
        rust_path.join(enum_type_name),
    );
}

fn oneof_type_to_extern_paths(
    extern_paths: &mut BTreeMap<ProtoPath, RustModulePath>,
    proto_path: &ProtoPath,
    rust_path: &RustModulePath,
    oneof_type: &OneofDescriptorProto,
) {
    let oneof_type_name = oneof_type
        .name
        .as_ref()
        .expect("Failed to get oneof type name");
    extern_paths.insert(
        proto_path.join(oneof_type_name),
        rust_path.join(oneof_type_name),
    );
}

/// The parsed command-line arguments.
struct Args {
    /// The path to the protoc binary.
    protoc: PathBuf,

    /// The path to the output directory.
    out_dir: PathBuf,

    /// The name of the crate.
    crate_name: String,

    /// The bazel label.
    label: String,

    /// The path to the package info file.
    package_info_file: PathBuf,

    /// The proto files to compile.
    proto_files: Vec<PathBuf>,

    /// Additional source files to append to the generated rust source.
    additional_srcs: Vec<PathBuf>,

    /// The include directories.
    includes: Vec<String>,

    /// Dependency descriptor sets.
    descriptor_set: PathBuf,

    /// The path to the generated lib.rs file.
    out_librs: PathBuf,

    /// The proto include paths.
    proto_paths: Vec<String>,

    /// Direct dependency crate names.
    direct_dep_crate_names: Vec<String>,

    /// The path to the rustfmt binary.
    rustfmt: Option<PathBuf>,

    /// Whether to generate tonic code.
    is_tonic: bool,

    /// Extra arguments to pass to protoc.
    extra_args: Vec<String>,
}

impl Args {
    /// Parse the command-line arguments.
    fn parse() -> Result<Args, String> {
        let mut protoc: Option<PathBuf> = None;
        let mut out_dir: Option<PathBuf> = None;
        let mut crate_name: Option<String> = None;
        let mut package_info_file: Option<PathBuf> = None;
        let mut proto_files: Vec<PathBuf> = Vec::new();
        let mut additional_srcs: Vec<PathBuf> = Vec::new();
        let mut includes = Vec::new();
        let mut descriptor_set = None;
        let mut out_librs: Option<PathBuf> = None;
        let mut rustfmt: Option<PathBuf> = None;
        let mut proto_paths = Vec::new();
        let mut label: Option<String> = None;
        let mut tonic_or_prost_opts = Vec::new();
        let mut direct_dep_crate_names = Vec::new();
        let mut is_tonic = false;

        let mut extra_args = Vec::new();

        let mut handle_arg = |arg: String| {
            if !arg.starts_with('-') {
                proto_files.push(PathBuf::from(arg));
                return;
            }

            if arg.starts_with("-I") {
                includes.push(
                    arg.strip_prefix("-I")
                        .expect("Failed to strip -I")
                        .to_string(),
                );
                return;
            }

            if arg == "--is_tonic" {
                is_tonic = true;
                return;
            }

            if !arg.contains('=') {
                extra_args.push(arg);
                return;
            }

            let parts = arg.split_once('=').expect("Failed to split argument on =");
            match parts {
                ("--protoc", value) => {
                    protoc = Some(PathBuf::from(value));
                }
                ("--prost_out", value) => {
                    out_dir = Some(PathBuf::from(value));
                }
                ("--package_info_output", value) => {
                    let (key, value) = value
                        .split_once('=')
                        .map(|(a, b)| (a.to_string(), PathBuf::from(b)))
                        .expect("Failed to parse package info output");
                    crate_name = Some(key);
                    package_info_file = Some(value);
                }
                ("--deps_info", value) => {
                    for line in fs::read_to_string(value)
                        .expect("Failed to read file")
                        .lines()
                    {
                        let path = PathBuf::from(line.trim());
                        for flag in fs::read_to_string(path)
                            .expect("Failed to read file")
                            .lines()
                        {
                            tonic_or_prost_opts.push(format!("extern_path={}", flag.trim()));
                        }
                    }
                }
                ("--additional_srcs", value) => {
                    if !value.is_empty() {
                        additional_srcs
                            .extend(value.split(',').map(PathBuf::from).collect::<Vec<_>>());
                    }
                }
                ("--direct_dep_crate_names", value) => {
                    if value.trim().is_empty() {
                        return;
                    }

                    direct_dep_crate_names = value.split(',').map(|s| s.to_string()).collect();
                }
                ("--descriptor_set", value) => {
                    descriptor_set = Some(PathBuf::from(value));
                }
                ("--out_librs", value) => {
                    out_librs = Some(PathBuf::from(value));
                }
                ("--rustfmt", value) => {
                    rustfmt = Some(PathBuf::from(value));
                }
                ("--proto_path", value) => {
                    proto_paths.push(value.to_string());
                }
                ("--label", value) => {
                    label = Some(value.to_string());
                }
                (arg, value) => {
                    extra_args.push(format!("{}={}", arg, value));
                }
            }
        };

        // Iterate over the given command line arguments parsing out arguments
        // for the process runner and arguments for protoc and potentially spawn
        // additional arguments needed by prost.
        for arg in env::args().skip(1) {
            if let Some(path) = arg.strip_prefix('@') {
                // handle argfile
                let file = std::fs::File::open(path)
                    .map_err(|_| format!("could not open argfile: {}", arg))?;
                for line in std::io::BufReader::new(file).lines() {
                    handle_arg(line.map_err(|_| format!("could not read argfile: {}", arg))?);
                }
            } else {
                handle_arg(arg);
            }
        }

        for tonic_or_prost_opt in tonic_or_prost_opts {
            extra_args.push(format!("--prost_opt={}", tonic_or_prost_opt));
            if is_tonic {
                extra_args.push(format!("--tonic_opt={}", tonic_or_prost_opt));
            }
        }

        if protoc.is_none() {
            return Err(
                "No `--protoc` value was found. Unable to parse path to proto compiler."
                    .to_string(),
            );
        }
        if out_dir.is_none() {
            return Err(
                "No `--prost_out` value was found. Unable to parse output directory.".to_string(),
            );
        }
        if crate_name.is_none() {
            return Err(
                "No `--package_info_output` value was found. Unable to parse target crate name."
                    .to_string(),
            );
        }
        if package_info_file.is_none() {
            return Err("No `--package_info_output` value was found. Unable to parse package info output file.".to_string());
        }
        if out_librs.is_none() {
            return Err("No `--out_librs` value was found. Unable to parse the output location for all combined prost outputs.".to_string());
        }
        if descriptor_set.is_none() {
            return Err(
                "No `--descriptor_set` value was found. Unable to parse descriptor set path."
                    .to_string(),
            );
        }
        if label.is_none() {
            return Err(
                "No `--label` value was found. Unable to parse the label of the target crate."
                    .to_string(),
            );
        }

        Ok(Args {
            protoc: protoc.unwrap(),
            out_dir: out_dir.unwrap(),
            crate_name: crate_name.unwrap(),
            package_info_file: package_info_file.unwrap(),
            proto_files,
            additional_srcs,
            includes,
            descriptor_set: descriptor_set.unwrap(),
            out_librs: out_librs.unwrap(),
            rustfmt,
            proto_paths,
            direct_dep_crate_names,
            is_tonic,
            label: label.unwrap(),
            extra_args,
        })
    }
}

/// Get the output directory with the label suffixed.
fn get_output_dir(out_dir: &Path, label: &str) -> PathBuf {
    let label_as_path = label
        .replace('@', "")
        .replace("//", "_")
        .replace(['/', ':'], "_");
    PathBuf::from(format!(
        "{}/prost-build-{}",
        out_dir.display(),
        label_as_path
    ))
}

/// Get the output directory with the label suffixed, and create it if it doesn't exist.
///
/// This will remove the directory first if it already exists.
fn get_and_create_output_dir(out_dir: &Path, label: &str) -> PathBuf {
    let out_dir = get_output_dir(out_dir, label);
    if out_dir.exists() {
        fs::remove_dir_all(&out_dir).expect("Failed to remove old output directory");
    }
    fs::create_dir_all(&out_dir).expect("Failed to create output directory");
    out_dir
}

/// Parse the descriptor set file into a `FileDescriptorSet`.
fn parse_descriptor_set_file(descriptor_set_path: &PathBuf) -> FileDescriptorSet {
    let descriptor_set_bytes =
        fs::read(descriptor_set_path).expect("Failed to read descriptor set");
    let descriptor_set = FileDescriptorSet::decode(descriptor_set_bytes.as_slice())
        .expect("Failed to decode descriptor set");

    descriptor_set
}

/// Get the package name from the descriptor set.
fn get_package_name(descriptor_set: &FileDescriptorSet) -> Option<String> {
    let mut package_name = None;

    for file in &descriptor_set.file {
        if let Some(package) = &file.package {
            package_name = Some(package.clone());
            break;
        }
    }

    package_name
}

/// Whether the proto file should expect to generate a .rs file.
///
/// If the proto file contains any messages, enums, or services, then it should generate a rust file.
/// If the proto file only contains extensions, then it will not generate any rust files.
fn expect_fs_file_to_be_generated(descriptor_set: &FileDescriptorSet) -> bool {
    let mut expect_rs = false;

    for file in descriptor_set.file.iter() {
        let has_messages = !file.message_type.is_empty();
        let has_enums = !file.enum_type.is_empty();
        let has_services = !file.service.is_empty();
        let has_extensions = !file.extension.is_empty();

        let has_definition = has_messages || has_enums || has_services;

        if has_definition {
            return true;
        } else if !has_definition && !has_extensions {
            expect_rs = true;
        }
    }

    expect_rs
}

/// Whether the proto file should expect to generate service definitions.
fn has_services(descriptor_set: &FileDescriptorSet) -> bool {
    descriptor_set
        .file
        .iter()
        .any(|file| !file.service.is_empty())
}

fn main() {
    let Args {
        protoc,
        out_dir,
        crate_name,
        label,
        package_info_file,
        proto_files,
        additional_srcs,
        includes,
        descriptor_set,
        out_librs,
        rustfmt,
        proto_paths,
        direct_dep_crate_names,
        is_tonic,
        extra_args,
    } = Args::parse().expect("Failed to parse args");

    let out_dir = get_and_create_output_dir(&out_dir, &label);

    let descriptor_set = parse_descriptor_set_file(&descriptor_set);
    let package_name = get_package_name(&descriptor_set).unwrap_or_default();
    let expect_rs = expect_fs_file_to_be_generated(&descriptor_set);
    let has_services = has_services(&descriptor_set);
    let additional_content = additional_srcs
        .into_iter()
        .map(|f| {
            fs::read_to_string(&f).unwrap_or_else(|e| {
                panic!(
                    "Failed to read additional source file: `{}`\n{:?}",
                    f.display(),
                    e
                )
            })
        })
        .collect::<Vec<_>>()
        .join("\n");

    if has_services && !is_tonic {
        eprintln!("Warning: Service definitions will not be generated because the prost toolchain did not define a tonic plugin.");
    }

    let tmp_dir = out_dir.parent().unwrap().join(format!(
        "{}.tmp",
        out_dir.file_name().unwrap().to_string_lossy()
    ));
    if tmp_dir.exists() {
        fs::remove_dir_all(&tmp_dir).unwrap_or_else(|e| {
            panic!("Failed to delete directory: {}\n{:?}", tmp_dir.display(), e)
        });
    }
    fs::create_dir_all(&tmp_dir)
        .unwrap_or_else(|e| panic!("Failed to create directory: {}\n{:?}", tmp_dir.display(), e));

    let args_file = out_dir.join("args.txt");
    let mut args = Vec::new();

    args.push(format!("--prost_out={}", out_dir.display()));
    if is_tonic {
        args.push(format!("--tonic_out={}", out_dir.display()));
    }
    args.extend(extra_args);
    args.extend(
        proto_paths
            .iter()
            .map(|proto_path| format!("--proto_path={}", proto_path)),
    );
    args.extend(includes.iter().map(|include| format!("-I{}", include)));
    args.extend(proto_files.iter().map(|f| f.to_string_lossy().to_string()));

    fs::write(&args_file, args.join("\n")).unwrap_or_else(|e| {
        panic!(
            "Failed to write args file: {}\n{:?}",
            args_file.display(),
            e
        )
    });
    let mut cmd = process::Command::new(protoc);
    cmd.arg(format!("@{}", args_file.display()));

    let status_result = cmd.status();

    fs::remove_dir_all(&tmp_dir)
        .unwrap_or_else(|e| panic!("Failed to delete directory: {}\n{:?}", tmp_dir.display(), e));

    let status = status_result.unwrap_or_else(|e| {
        panic!(
            "Failed to spawn protoc process\n{:#?}\n{} -- {:#?}\n{:?}",
            cmd,
            args_file.display(),
            args,
            e
        )
    });
    if !status.success() {
        panic!(
            "protoc failed with status: {}",
            status.code().expect("failed to get exit code")
        );
    }

    // Not all proto files will consistently produce `.rs` or `.tonic.rs` files. This is
    // caused by the proto file being transpiled not having an RPC service or other protos
    // defined (a natural and expected situation). To guarantee consistent outputs, all
    // `.rs` files are either renamed to `.tonic.rs` if there is no `.tonic.rs` or prepended
    // to the existing `.tonic.rs`.
    if is_tonic {
        let tonic_files: BTreeSet<PathBuf> = find_generated_rust_files(&out_dir);

        for tonic_file in tonic_files.iter() {
            let tonic_path_str = tonic_file.to_str().expect("Failed to convert to str");
            let filename = tonic_file
                .file_name()
                .expect("Failed to get file name")
                .to_str()
                .expect("Failed to convert to str");

            let is_tonic_file = filename.ends_with(".tonic.rs");

            if is_tonic_file {
                let rs_file_str = format!(
                    "{}.rs",
                    tonic_path_str
                        .strip_suffix(".tonic.rs")
                        .expect("Failed to strip suffix.")
                );
                let rs_file = PathBuf::from(&rs_file_str);

                if rs_file.exists() {
                    let rs_content = fs::read_to_string(&rs_file).expect("Failed to read file.");
                    let tonic_content =
                        fs::read_to_string(tonic_file).expect("Failed to read file.");
                    fs::write(tonic_file, format!("{}\n{}", rs_content, tonic_content))
                        .expect("Failed to write file.");
                    fs::remove_file(&rs_file).unwrap_or_else(|err| {
                        panic!("Failed to remove file: {err:?}: {rs_file:?}")
                    });
                }
            } else {
                let real_tonic_file = PathBuf::from(format!(
                    "{}.tonic.rs",
                    tonic_path_str
                        .strip_suffix(".rs")
                        .expect("Failed to strip suffix.")
                ));
                if real_tonic_file.exists() {
                    continue;
                }
                fs::rename(tonic_file, &real_tonic_file).unwrap_or_else(|err| {
                    panic!("Failed to rename file: {err:?}: {tonic_file:?} -> {real_tonic_file:?}");
                });
            }
        }
    }

    // Locate all prost-generated outputs.
    let mut rust_files = find_generated_rust_files(&out_dir);
    if rust_files.is_empty() {
        if expect_rs {
            panic!("No .rs files were generated by prost.");
        } else {
            let file_stem = if package_name.is_empty() {
                "_"
            } else {
                &package_name
            };
            let file_stem = format!("{}{}", file_stem, if is_tonic { ".tonic" } else { "" });
            let empty_rs_file = out_dir.join(format!("{}.rs", file_stem));
            fs::write(&empty_rs_file, "").expect("Failed to write file.");
            rust_files.insert(empty_rs_file);
        }
    }

    let extern_paths = get_extern_paths(&descriptor_set, &crate_name)
        .expect("Failed to compute proto package info");

    // Write outputs
    fs::write(
        &out_librs,
        generate_lib_rs(
            &rust_files,
            is_tonic,
            direct_dep_crate_names,
            additional_content,
        ),
    )
    .expect("Failed to write file.");
    fs::write(
        package_info_file,
        extern_paths
            .into_iter()
            .map(|(proto_path, rust_path)| format!(".{}=::{}", proto_path, rust_path))
            .collect::<Vec<_>>()
            .join("\n"),
    )
    .expect("Failed to write file.");

    // Finally run rustfmt on the output lib.rs file
    if let Some(rustfmt) = rustfmt {
        let fmt_status = process::Command::new(rustfmt)
            .arg("--edition")
            .arg("2021")
            .arg("--quiet")
            .arg(&out_librs)
            .status()
            .expect("Failed to spawn rustfmt process");
        if !fmt_status.success() {
            panic!(
                "rustfmt failed with exit code: {}",
                fmt_status.code().expect("Failed to get exit code")
            );
        }
    }
}

/// Rust built-in keywords and reserved keywords.
const RUST_KEYWORDS: [&str; 51] = [
    "abstract", "as", "async", "await", "become", "box", "break", "const", "continue", "crate",
    "do", "dyn", "else", "enum", "extern", "false", "final", "fn", "for", "if", "impl", "in",
    "let", "loop", "macro", "match", "mod", "move", "mut", "override", "priv", "pub", "ref",
    "return", "self", "Self", "static", "struct", "super", "trait", "true", "try", "type",
    "typeof", "unsafe", "unsized", "use", "virtual", "where", "while", "yield",
];

/// Returns true if the given string is a Rust keyword.
fn is_keyword(s: &str) -> bool {
    RUST_KEYWORDS.contains(&s)
}

/// Escapes a Rust keyword by prefixing it with `r#`.
fn escape_keyword(s: String) -> String {
    if is_keyword(&s) {
        return format!("r#{s}");
    }
    s
}

#[cfg(test)]
mod test {

    use super::*;

    use prost_types::{FieldDescriptorProto, ServiceDescriptorProto};

    #[test]
    fn oneof_type_to_extern_paths_test() {
        let oneof_descriptor = OneofDescriptorProto {
            name: Some("Foo".to_string()),
            ..OneofDescriptorProto::default()
        };

        {
            let mut extern_paths = BTreeMap::new();
            oneof_type_to_extern_paths(
                &mut extern_paths,
                &ProtoPath::from("bar"),
                &RustModulePath::from("bar"),
                &oneof_descriptor,
            );

            assert_eq!(extern_paths.len(), 1);
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.Foo")),
                Some(&RustModulePath::from("bar::Foo"))
            );
        }

        {
            let mut extern_paths = BTreeMap::new();
            oneof_type_to_extern_paths(
                &mut extern_paths,
                &ProtoPath::from("bar.baz"),
                &RustModulePath::from("bar::baz"),
                &oneof_descriptor,
            );

            assert_eq!(extern_paths.len(), 1);
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.baz.Foo")),
                Some(&RustModulePath::from("bar::baz::Foo"))
            );
        }
    }

    #[test]
    fn enum_type_to_extern_paths_test() {
        let enum_descriptor = EnumDescriptorProto {
            name: Some("Foo".to_string()),
            ..EnumDescriptorProto::default()
        };

        {
            let mut extern_paths = BTreeMap::new();
            enum_type_to_extern_paths(
                &mut extern_paths,
                &ProtoPath::from("bar"),
                &RustModulePath::from("bar"),
                &enum_descriptor,
            );

            assert_eq!(extern_paths.len(), 1);
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.Foo")),
                Some(&RustModulePath::from("bar::Foo"))
            );
        }

        {
            let mut extern_paths = BTreeMap::new();
            enum_type_to_extern_paths(
                &mut extern_paths,
                &ProtoPath::from("bar.baz"),
                &RustModulePath::from("bar::baz"),
                &enum_descriptor,
            );

            assert_eq!(extern_paths.len(), 1);
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.baz.Foo")),
                Some(&RustModulePath::from("bar::baz::Foo"))
            );
        }
    }

    #[test]
    fn message_type_to_extern_paths_test() {
        let message_descriptor = DescriptorProto {
            name: Some("Foo".to_string()),
            nested_type: vec![
                DescriptorProto {
                    name: Some("Bar".to_string()),
                    ..DescriptorProto::default()
                },
                DescriptorProto {
                    name: Some("Nested".to_string()),
                    nested_type: vec![DescriptorProto {
                        name: Some("Baz".to_string()),
                        enum_type: vec![EnumDescriptorProto {
                            name: Some("Chuck".to_string()),
                            ..EnumDescriptorProto::default()
                        }],
                        ..DescriptorProto::default()
                    }],
                    ..DescriptorProto::default()
                },
            ],
            enum_type: vec![EnumDescriptorProto {
                name: Some("Qux".to_string()),
                ..EnumDescriptorProto::default()
            }],
            ..DescriptorProto::default()
        };

        {
            let mut extern_paths = BTreeMap::new();
            message_type_to_extern_paths(
                &mut extern_paths,
                &ProtoPath::from("bar"),
                &RustModulePath::from("bar"),
                &message_descriptor,
            );
            assert_eq!(extern_paths.len(), 6);
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.Foo")),
                Some(&RustModulePath::from("bar::Foo"))
            );
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.foo.Bar")),
                Some(&RustModulePath::from("bar::foo::Bar"))
            );
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.foo.Nested")),
                Some(&RustModulePath::from("bar::foo::Nested"))
            );
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.foo.nested.Baz")),
                Some(&RustModulePath::from("bar::foo::nested::Baz"))
            );
        }

        {
            let mut extern_paths = BTreeMap::new();
            message_type_to_extern_paths(
                &mut extern_paths,
                &ProtoPath::from("bar.bob"),
                &RustModulePath::from("bar::bob"),
                &message_descriptor,
            );
            assert_eq!(extern_paths.len(), 6);
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.bob.Foo")),
                Some(&RustModulePath::from("bar::bob::Foo"))
            );
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.bob.foo.Bar")),
                Some(&RustModulePath::from("bar::bob::foo::Bar"))
            );
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.bob.foo.Nested")),
                Some(&RustModulePath::from("bar::bob::foo::Nested"))
            );
            assert_eq!(
                extern_paths.get(&ProtoPath::from("bar.bob.foo.nested.Baz")),
                Some(&RustModulePath::from("bar::bob::foo::nested::Baz"))
            );
        }
    }

    #[test]
    fn proto_path_test() {
        {
            let proto_path = ProtoPath::from("");
            assert_eq!(proto_path.to_string(), "");
            assert_eq!(proto_path.join("foo"), ProtoPath::from("foo"));
        }
        {
            let proto_path = ProtoPath::from("foo");
            assert_eq!(proto_path.to_string(), "foo");
            assert_eq!(proto_path.join(""), ProtoPath::from("foo"));
        }
        {
            let proto_path = ProtoPath::from("foo");
            assert_eq!(proto_path.to_string(), "foo");
            assert_eq!(proto_path.join("bar"), ProtoPath::from("foo.bar"));
        }
        {
            let proto_path = ProtoPath::from("foo.bar");
            assert_eq!(proto_path.to_string(), "foo.bar");
            assert_eq!(proto_path.join("baz"), ProtoPath::from("foo.bar.baz"));
        }
        {
            let proto_path = ProtoPath::from("Foo.baR");
            assert_eq!(proto_path.to_string(), "Foo.baR");
            assert_eq!(proto_path.join("baz"), ProtoPath::from("Foo.baR.baz"));
        }
    }

    #[test]
    fn rust_module_path_test() {
        {
            let rust_module_path = RustModulePath::from("");
            assert_eq!(rust_module_path.to_string(), "");
            assert_eq!(rust_module_path.join("foo"), RustModulePath::from("foo"));
        }
        {
            let rust_module_path = RustModulePath::from("foo");
            assert_eq!(rust_module_path.to_string(), "foo");
            assert_eq!(rust_module_path.join(""), RustModulePath::from("foo"));
        }
        {
            let rust_module_path = RustModulePath::from("foo");
            assert_eq!(rust_module_path.to_string(), "foo");
            assert_eq!(
                rust_module_path.join("bar"),
                RustModulePath::from("foo::bar")
            );
        }
        {
            let rust_module_path = RustModulePath::from("foo::bar");
            assert_eq!(rust_module_path.to_string(), "foo::bar");
            assert_eq!(
                rust_module_path.join("baz"),
                RustModulePath::from("foo::bar::baz")
            );
        }
    }

    #[test]
    fn expect_fs_file_to_be_generated_test() {
        {
            // Empty descriptor set should create a file.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(expect_fs_file_to_be_generated(&descriptor_set));
        }
        {
            // Descriptor set with only message should create a file.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    message_type: vec![DescriptorProto {
                        name: Some("Foo".to_string()),
                        ..DescriptorProto::default()
                    }],
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(expect_fs_file_to_be_generated(&descriptor_set));
        }
        {
            // Descriptor set with only enum should create a file.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    enum_type: vec![EnumDescriptorProto {
                        name: Some("Foo".to_string()),
                        ..EnumDescriptorProto::default()
                    }],
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(expect_fs_file_to_be_generated(&descriptor_set));
        }
        {
            // Descriptor set with only service should create a file.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    service: vec![ServiceDescriptorProto {
                        name: Some("Foo".to_string()),
                        ..ServiceDescriptorProto::default()
                    }],
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(expect_fs_file_to_be_generated(&descriptor_set));
        }
        {
            // Descriptor set with only extensions should not create a file.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    extension: vec![FieldDescriptorProto {
                        name: Some("Foo".to_string()),
                        ..FieldDescriptorProto::default()
                    }],
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(!expect_fs_file_to_be_generated(&descriptor_set));
        }
    }

    #[test]
    fn has_services_test() {
        {
            // Empty file should not have services.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(!has_services(&descriptor_set));
        }
        {
            // File with only message should not have services.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    message_type: vec![DescriptorProto {
                        name: Some("Foo".to_string()),
                        ..DescriptorProto::default()
                    }],
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(!has_services(&descriptor_set));
        }
        {
            // File with services should have services.
            let descriptor_set = FileDescriptorSet {
                file: vec![FileDescriptorProto {
                    name: Some("foo.proto".to_string()),
                    service: vec![ServiceDescriptorProto {
                        name: Some("Foo".to_string()),
                        ..ServiceDescriptorProto::default()
                    }],
                    ..FileDescriptorProto::default()
                }],
            };
            assert!(has_services(&descriptor_set));
        }
    }

    #[test]
    fn get_package_name_test() {
        let descriptor_set = FileDescriptorSet {
            file: vec![FileDescriptorProto {
                name: Some("foo.proto".to_string()),
                package: Some("foo".to_string()),
                ..FileDescriptorProto::default()
            }],
        };

        assert_eq!(get_package_name(&descriptor_set), Some("foo".to_string()));
    }

    #[test]
    fn is_keyword_test() {
        let non_keywords = [
            "foo", "bar", "baz", "qux", "quux", "corge", "grault", "garply", "waldo", "fred",
            "plugh", "xyzzy", "thud",
        ];
        for non_keyword in &non_keywords {
            assert!(!is_keyword(non_keyword));
        }

        for keyword in &RUST_KEYWORDS {
            assert!(is_keyword(keyword));
        }
    }

    #[test]
    fn escape_keyword_test() {
        let non_keywords = [
            "foo", "bar", "baz", "qux", "quux", "corge", "grault", "garply", "waldo", "fred",
            "plugh", "xyzzy", "thud",
        ];
        for non_keyword in &non_keywords {
            assert_eq!(
                escape_keyword(non_keyword.to_string()),
                non_keyword.to_owned()
            );
        }

        for keyword in &RUST_KEYWORDS {
            assert_eq!(
                escape_keyword(keyword.to_string()),
                format!("r#{}", keyword)
            );
        }
    }
}
