"assertion rules to test metadata"

load("@aspect_bazel_lib//lib:diff_test.bzl", "diff_test")
load("@aspect_bazel_lib//lib:transitions.bzl", "platform_transition_filegroup")
load("@bazel_skylib//rules:native_binary.bzl", "native_test")
load("@bazel_skylib//rules:write_file.bzl", "write_file")

# THIS LOAD STATEMENT DEPENDS ON setup_assertion_repos.bzl
load("@docker_configure//:defs.bzl", "DOCKER_PLATFORM", "TARGET_COMPATIBLE_WITH")
load("//oci:defs.bzl", "oci_load")

DIGEST_CMD = """
image_path="$(location {image})"
manifest_digest=$$($(JQ_BIN) -r '.manifests[0].digest | sub(":"; "/")' $$image_path/index.json)
config_digest=$$($(JQ_BIN) -r '.config.digest | sub(":"; "/")' $$image_path/blobs/$$manifest_digest)

$(JQ_BIN) 'def pick(p): . as $$v | reduce path(p) as $$p ({{}}; setpath($$p; $$v | getpath($$p))); pick({keys})' "$$image_path/blobs/$$config_digest" > $@
"""

_DEFAULT_ = {"____I_WILL_NOT_MATCH_ANYTHING__": True}

# buildifier: disable=function-docstring-args
def assert_oci_config(
        name,
        image,
        entrypoint_eq = _DEFAULT_,
        cmd_eq = _DEFAULT_,
        env_eq = None,
        exposed_ports_eq = None,
        volumes_eq = None,
        user_eq = None,
        workdir_eq = None,
        architecture_eq = None,
        os_eq = None,
        variant_eq = None,
        labels_eq = None,
        created_eq = None,
        history_eq = None):
    "assert that an oci_image has specified config metadata according to https://github.com/opencontainers/image-spec/blob/main/config.md"
    pick = []

    config = {}

    # .config
    if entrypoint_eq != _DEFAULT_:
        config["Entrypoint"] = entrypoint_eq
    if cmd_eq != _DEFAULT_:
        config["Cmd"] = cmd_eq
    if env_eq:
        config["Env"] = ["=".join(e) for e in env_eq.items()]
    if workdir_eq:
        config["WorkingDir"] = workdir_eq
    if exposed_ports_eq:
        config["ExposedPorts"] = {port: {} for port in exposed_ports_eq}
    if volumes_eq:
        config["Volumes"] = {volume: {} for volume in volumes_eq}
    if user_eq:
        config["User"] = user_eq
    if labels_eq:
        config["Labels"] = labels_eq

    pick = [".config." + k for k in config.keys()]

    # .
    config_json = {}

    if os_eq:
        config_json["os"] = os_eq
    if architecture_eq:
        config_json["architecture"] = architecture_eq
    if variant_eq:
        config_json["variant"] = variant_eq

    if created_eq:
        config_json["created"] = created_eq
    if history_eq:
        config_json["history"] = history_eq

    pick += ["." + k for k in config_json.keys()]

    if len(config.keys()):
        config_json["config"] = config

    expected = name + "_json"
    write_file(
        name = expected,
        out = name + ".json",
        content = [
            json.encode_indent(config_json),
        ],
    )

    actual = name + "_config_json"
    native.genrule(
        name = actual,
        srcs = [image],
        outs = [name + ".config.json"],
        cmd = DIGEST_CMD.format(keys = ",".join(pick), image = image),
        toolchains = ["@jq_toolchains//:resolved_toolchain"],
    )

    native_test(
        name = name,
        data = [
            expected,
            actual,
        ],
        args = [
            "$(location %s)" % expected,
            "$(location %s)" % actual,
        ],
        src = "@multitool//tools/jd",
        out = name,
    )

# buildifier: disable=function-docstring-args
def assert_oci_image_command(
        name,
        image,
        args = [],
        tags = [],
        exit_code_eq = None,
        output_eq = None):
    "assert a that a container works with the given command."

    tag = "oci.local/assert/" + native.package_name().replace("/", "_") + ":latest"
    if DOCKER_PLATFORM:
        platform_transition_filegroup(
            name = name + "_runtime_image",
            srcs = [image],
            target_platform = DOCKER_PLATFORM,
            tags = tags + ["manual"],
        )
        image = ":" + name + "_runtime_image"
    oci_load(
        name = name + "_tarball",
        image = image,
        repo_tags = [tag],
        tags = tags + ["manual"],
    )

    docker_args = " ".join(['"' + arg + '"' for arg in ([tag] + args)])

    native.genrule(
        name = name + "_gen",
        output_to_bindir = True,
        cmd = """
docker=$(location @multitool//tools/docker)
$(location :{name}_tarball)
container_id=$$($$docker run -d {docker_args})
$$docker wait $$container_id > $(location :{name}_exit_code)
$$docker logs $$container_id > $(location :{name}_output)

""".format(name = name, docker_args = docker_args),
        outs = [
            name + "_output",
            name + "_exit_code",
        ],
        target_compatible_with = TARGET_COMPATIBLE_WITH,
        tools = [name + "_tarball", "@multitool//tools/docker"],
    )

    if output_eq:
        write_file(
            name = name + "_output_eq_expected",
            out = name + "_output_eq_expected.txt",
            content = [output_eq],
            tags = tags + ["manual"],
        )
        diff_test(
            name = name + "_assert_output_eq",
            file1 = name + "_output",
            file2 = name + "_output_eq_expected",
            tags = tags,
        )

    if exit_code_eq != None:
        write_file(
            name = name + "_exit_code_expected",
            out = name + "_exit_code_expected.txt",
            content = [str(exit_code_eq) + "\n"],
            tags = tags + ["manual"],
        )
        diff_test(
            name = name + "_assert_exit_code_eq",
            file1 = name + "_exit_code",
            file2 = name + "_exit_code_expected",
            tags = tags,
        )
