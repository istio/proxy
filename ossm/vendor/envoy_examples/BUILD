load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("@rules_pkg//pkg:pkg.bzl", "pkg_tar")
load("//:examples.bzl", "envoy_examples")

licenses(["notice"])  # Apache 2

# Excluding build tests - eg wasm - these should be run directly
EXAMPLE_TESTS = [
    "brotli",
    "cache",
    "cors",
    "csrf",
    "double-proxy",
    # "dynamic-config-cp",
    "dynamic-config-fs",
    "ext_authz",
    # "fault-injection",
    "front-proxy",
    "golang-http",
    # "golang-network",
    "grpc-bridge",
    "gzip",
    "jaeger-tracing",
    "kafka",
    "load-reporting-service",
    "locality-load-balancing",
    "local_ratelimit",
    "lua",
    "lua-cluster-specifier",
    "mysql",
    "opentelemetry",
    "postgres",
    "rbac",
    "redis",
    "route-mirror",
    "single-page-app",
    "skywalking",
    "tls",
    "tls-inspector",
    "tls-sni",
    "udp",
    "vrp-litmus",
    # "vrp-local",
    "websocket",
    "zipkin",
    "zstd",
]

filegroup(
    name = "configs",
    srcs = glob(
        [
            "**/*.yaml",
        ],
        exclude = [
            "cache/ci-responses.yaml",
            "cache/responses.yaml",
            "dynamic-config-fs/**/*",
            "jaeger-native-tracing/*",
            "opentelemetry/otel-collector-config.yaml",
            "**/*docker-compose*.yaml",
            # Contrib extensions tested over in contrib.
            "golang-http/*.yaml",
            "golang-network/*.yaml",
            "mysql/*",
            "postgres/*",
            "kafka/*.yaml",
        ],
    ) + ["@envoy-example-wasmcc//:configs"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "contrib_configs",
    srcs = glob(
        [
            "golang-http/*.yaml",
            "golang-network/*.yaml",
            "mysql/*.yaml",
            "postgres/*.yaml",
            "kafka/*.yaml",
        ],
        exclude = [
            "**/*docker-compose*.yaml",
        ],
    ),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "certs",
    srcs = glob(["_extra_certs/*.pem"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "docs_rst",
    srcs = glob(["**/example.rst"]) + ["@envoy-example-wasmcc//:example.rst"],
)

pkg_files(
    name = "examples_files",
    srcs = [":files"],
    prefix = "_include",
    strip_prefix = "/",
)

genrule(
    name = "examples_docs",
    srcs = [":docs_rst"],
    outs = ["examples_docs.tar.gz"],
    cmd = """
    TEMP=$$(mktemp -d)
    for location in $(locations :docs_rst); do
        if [[ "$$location" == *"/envoy-example"* ]]; then
            example="$$(echo "$$location" | cut -d- -f3- | cut -d/ -f1)"
        else
            example=$$(echo $$location | sed -e 's#^external/[^/]*/##' | cut -d/ -f1)
        fi
        cp -aL $$location "$${TEMP}/$${example}.rst"
        echo "    $${example}" >> "$${TEMP}/_toctree.rst"
    done
    echo ".. toctree::" > "$${TEMP}/toctree.rst"
    echo "    :maxdepth: 1" >> "$${TEMP}/toctree.rst"
    echo "" >> "$${TEMP}/toctree.rst"
    cat "$${TEMP}/_toctree.rst" | sort >> "$${TEMP}/toctree.rst"
    rm "$${TEMP}/_toctree.rst"
    tar chzf $@ -C $${TEMP} .
    """,
)

filegroup(
    name = "lua",
    srcs = glob(["**/*.lua"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "files",
    srcs = glob(
        ["**/*"],
        exclude = [
            "**/*~",
            "**/.*",
            "**/#*",
            ".*/**/*",
            "BUILD",
            "README.md",
            "WORKSPACE",
            "bazel-*/**/*",
            "**/node_modules/**",
            "**/*.rst",
            "win32*",
        ],
    ),
)

pkg_tar(
    name = "docs",
    srcs = [":examples_files"],
    extension = "tar.gz",
    package_dir = "start/sandboxes",
    deps = [
        ":examples_docs",
        "@envoy-example-wasmcc//:includes",
    ],
    visibility = ["//visibility:public"],
)

envoy_examples(
    examples = EXAMPLE_TESTS,
)
