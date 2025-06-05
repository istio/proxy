"""External repository for `generated_inputs` tests"""

_BUILD_FILE_CONTENT = """
load("@rules_rust//rust:defs.bzl", "rust_library")
load("@bazel_skylib//rules:write_file.bzl", "write_file")

write_file(
    name = "generate_src",
    out = "src.rs",
    content = ["pub fn forty_two() -> i32 { 42 }"],
)

rust_library(
    name = "generated_inputs_external_repo",
    srcs = [
        "lib.rs",
        ":generate_src",
    ],
    edition = "2021",
    visibility = ["//visibility:public"],
)
"""

_LIB_RS_CONTENT = """
mod src;

pub fn forty_two_from_generated_src() -> String {
    format!("{}", src::forty_two())
}

#[cfg(test)]
mod test {
    #[test]
    fn test_forty_two_as_string() {
        assert_eq!(super::forty_two_from_generated_src(), "42");
    }
}
"""

def _generated_inputs_in_external_repo_impl(repository_ctx):
    # Create repository files (not in the root directory)
    repo_path = repository_ctx.path("lib")
    repository_ctx.file(
        "{}/BUILD.bazel".format(repo_path),
        content = _BUILD_FILE_CONTENT,
    )
    repository_ctx.file(
        "{}/lib.rs".format(repo_path),
        content = _LIB_RS_CONTENT,
    )

_generated_inputs_in_external_repo = repository_rule(
    implementation = _generated_inputs_in_external_repo_impl,
    doc = (
        "A test repository rule providing a Rust library using generated sources"
    ),
)

def generated_inputs_in_external_repo():
    """Define the a test repository with Rust library using generated sources"""

    _generated_inputs_in_external_repo(
        name = "generated_inputs_in_external_repo",
    )
    return [struct(repo = "generated_inputs_in_external_repo", is_dev_dep = True)]
