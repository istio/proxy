workspace(
    # see https://docs.bazel.build/versions/main/skylark/deploying.html#workspace
    name = "aspect_rules_js",
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Override rules_nodejs to v6 to test the latest and recommended versional internally,
# while keeping v5 in rules_js_dependencies() to avoid breaking changes.
# TODO(2.0): change minimum to v6 in repositories.bzl
http_archive(
    name = "rules_nodejs",
    sha256 = "dddd60acc3f2f30359bef502c9d788f67e33814b0ddd99aa27c5a15eb7a41b8c",
    strip_prefix = "rules_nodejs-6.1.0",
    url = "https://github.com/bazelbuild/rules_nodejs/releases/download/v6.1.0/rules_nodejs-v6.1.0.tar.gz",
)

load("//js:dev_repositories.bzl", "rules_js_dev_dependencies")

rules_js_dev_dependencies()

load("//js:repositories.bzl", "rules_js_dependencies")

rules_js_dependencies()

load("@aspect_bazel_lib//lib:repositories.bzl", "aspect_bazel_lib_dependencies", "register_coreutils_toolchains", "register_jq_toolchains")

aspect_bazel_lib_dependencies()

register_jq_toolchains()

register_coreutils_toolchains()

load("@rules_nodejs//nodejs:repositories.bzl", "nodejs_register_toolchains")

nodejs_register_toolchains(
    name = "nodejs",
    node_version = "16.14.2",
)

# Alternate toolchains for testing across versions
nodejs_register_toolchains(
    name = "node16",
    node_version = "16.13.1",
)

nodejs_register_toolchains(
    name = "node18",
    node_version = "18.13.0",
)

nodejs_register_toolchains(
    name = "node20",
    node_version = "20.11.1",
)

load("@bazel_skylib//lib:unittest.bzl", "register_unittest_toolchains")

register_unittest_toolchains()

load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")

host_repo(name = "aspect_bazel_lib_host")

############################################
# Gazelle, for generating bzl_library targets

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

go_rules_dependencies()

go_register_toolchains(version = "1.20.5")

gazelle_dependencies()

############################################
# Example npm dependencies

load("@aspect_rules_js//npm:repositories.bzl", "npm_import", "npm_translate_lock")

npm_translate_lock(
    name = "npm",
    bins = {
        # derived from "bin" attribute in node_modules/typescript/package.json
        "typescript": {
            "tsc": "./bin/tsc",
            "tsserver": "./bin/tsserver",
        },
    },
    custom_postinstalls = {
        "@aspect-test/c": "echo moo > cow.txt",
        "@aspect-test/c@2.0.2": "echo mooo >> cow.txt",
    },
    data = [
        "//:examples/npm_deps/patches/meaning-of-life@1.0.0-pnpm.patch",
        "//:package.json",
        "//:pnpm-workspace.yaml",
        "//examples/js_binary:package.json",
        "//examples/macro:package.json",
        "//examples/npm_deps:package.json",
        "//examples/npm_package/libs/lib_a:package.json",
        "//examples/npm_package/packages/pkg_a:package.json",
        "//examples/npm_package/packages/pkg_b:package.json",
        "//examples/webpack_cli:package.json",
        "//js/private/coverage/bundle:package.json",
        "//js/private/image:package.json",
        "//js/private/test/image:package.json",
        "//js/private/test/js_run_devserver:package.json",
        "//js/private/worker/src:package.json",
        "//npm/private/test:package.json",
        "//npm/private/test:vendored/lodash-4.17.21.tgz",
        "//npm/private/test/npm_package:package.json",
        "//npm/private/test/vendored/is-odd:package.json",
        "//npm/private/test/vendored/semver-max:package.json",
    ],
    generate_bzl_library_targets = True,
    lifecycle_hooks = {
        # We fetch @kubernetes/client-node from source and it has a `prepare` lifecycle hook that needs to be run
        # which runs the `build` package.json script: https://github.com/kubernetes-client/javascript/blob/fc681991e61c6808dd26012a2331f83671a11218/package.json#L28.
        # Here we run run build so we just run `tsc` instead of `npm run build` which ends up just running `tsc`.
        "@kubernetes/client-node": ["build"],
        # 'install' hook fails as it assumes the following path to `node-pre-gyp`: ./node_modules/.bin/node-pre-gyp
        # https://github.com/stultuss/protoc-gen-grpc-ts/blob/53d52a9d0e1fe3cbe930dec5581eca89b3dde807/package.json#L28
        "protoc-gen-grpc@2.0.3": [],
    },
    lifecycle_hooks_execution_requirements = {
        "*": [
            "no-sandbox",
        ],
        # If @kubernetes/client-node is not sandboxed, will fail with
        # ```
        # src/azure_auth.ts(97,43): error TS2575: No overload expects 2 arguments, but overloads do exist that expect either 1 or 4 arguments.
        # src/azure_auth.ts(98,34): error TS2575: No overload expects 2 arguments, but overloads do exist that expect either 1 or 4 arguments.
        # src/gcp_auth.ts(93,43): error TS2575: No overload expects 2 arguments, but overloads do exist that expect either 1 or 4 arguments.
        # src/gcp_auth.ts(94,34): error TS2575: No overload expects 2 arguments, but overloads do exist that expect either 1 or 4 arguments.
        # ```
        # since a `jsonpath-plus@7.2.0` that is newer then the transitive dep `jsonpath-plus@0.19.0` is found outside of the sandbox that
        # includes typings that don't match the 0.19.0 "any" usage.
        "@kubernetes/client-node": [],
        "@figma/nodegit": [
            "no-sandbox",
            "requires-network",
        ],
        "esbuild": [
            "no-sandbox",
            "requires-network",
        ],
        "segfault-handler": [
            "no-sandbox",
            "requires-network",
        ],
        "puppeteer": [
            "no-sandbox",
            "requires-network",
        ],
    },
    npmrc = "//:.npmrc",
    package_visibility = {
        "unused": ["//visibility:private"],
        "@mycorp/pkg-a": ["//examples:__subpackages__"],
    },
    patch_args = {
        "*": ["-p1"],
    },
    patches = {
        "meaning-of-life@1.0.0": ["//examples/npm_deps:patches/meaning-of-life@1.0.0-after_pnpm.patch"],
    },
    pnpm_lock = "//:pnpm-lock.yaml",
    # Use a version that's not vendored into rules_js by providing a (version, integrity) tuple.
    # curl --silent https://registry.npmjs.org/pnpm | jq '.versions["8.6.11"].dist.integrity'
    pnpm_version = ("8.6.11", "sha512-jqknppuj45tDzJsLcLqkAxytBHZXIx9JTYkGNq0/7pSRggpio9wRxTDj4NA2ilOHPlJ5BVjB5Ij5dx65woMi5A=="),
    public_hoist_packages = {
        # Instructs the linker to hoist the ms@2.1.3 npm package to `node_modules/ms` in the `examples/npm_deps` package.
        # Similar to adding `public-hoist-pattern[]=ms` in .npmrc but with control over which version to hoist and where
        # to hoist it. This hoisted package can be referenced by the label `//examples/npm_deps:node_modules/ms` same as
        # other direct dependencies in the `examples/npm_deps/package.json`.
        "ms@2.1.3": ["examples/npm_deps"],
    },
    replace_packages = {
        "chalk@5.0.1": "@chalk_501//:pkg",
    },
    update_pnpm_lock = True,
    verify_node_modules_ignored = "//:.bazelignore",
    verify_patches = "//examples/npm_deps/patches:patches",
)

load("@npm//:repositories.bzl", "npm_repositories")

# Declares npm_import rules from the pnpm-lock.yaml file
npm_repositories()

# As an example, manually import a package using explicit coordinates.
# Just a demonstration of the syntax de-sugaring.
npm_import(
    name = "acorn__8.4.0",
    bins = {"acorn": "./bin/acorn"},
    integrity = "sha512-ULr0LDaEqQrMFGyQ3bhJkLsbtrQ8QibAseGZeaSUiT/6zb9IvIkomWHJIvgvwad+hinRAgsI51JcWk2yvwyL+w==",
    package = "acorn",
    # Root package where to link the virtual store
    root_package = "",
    version = "8.4.0",
)

# Buildifier
load("@buildifier_prebuilt//:deps.bzl", "buildifier_prebuilt_deps")

buildifier_prebuilt_deps()

load("@buildifier_prebuilt//:defs.bzl", "buildifier_prebuilt_register_toolchains")

buildifier_prebuilt_register_toolchains()

# rules_lint
load(
    "@aspect_rules_lint//format:repositories.bzl",
    "fetch_shfmt",
    "fetch_terraform",
)

fetch_shfmt()

fetch_terraform()

load("@com_grail_bazel_toolchain//toolchain:deps.bzl", "bazel_toolchain_dependencies")

bazel_toolchain_dependencies()

load("@com_grail_bazel_toolchain//toolchain:rules.bzl", "llvm_toolchain")

llvm_toolchain(
    name = "llvm_toolchain",
    llvm_version = "14.0.0",
    sha256 = {
        "darwin-aarch64": "1b8975db6b638b308c1ee437291f44cf8f67a2fb926eb2e6464efd180e843368",
        "linux-x86_64": "564fcbd79c991e93fdf75f262fa7ac6553ec1dd04622f5d7db2a764c5dc7fac6",
    },
    strip_prefix = {
        "darwin-aarch64": "clang+llvm-14.0.0-arm64-apple-darwin",
        "linux-x86_64": "clang+llvm-14.0.0-x86_64-linux-gnu",
    },
    sysroot = {
        "linux-aarch64": "@org_chromium_sysroot_linux_arm64//:sysroot",
        "linux-x86_64": "@org_chromium_sysroot_linux_x86_64//:sysroot",
        "darwin-aarch64": "@sysroot_darwin_universal//:sysroot",
        "darwin-x86_64": "@sysroot_darwin_universal//:sysroot",
    },
    urls = {
        "darwin-aarch64": ["https://github.com/aspect-forks/llvm-project/releases/download/aspect-release-14.0.0/clang+llvm-14.0.0-arm64-apple-darwin.tar.xz"],
        "linux-x86_64": ["https://github.com/aspect-forks/llvm-project/releases/download/aspect-release-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu.tar.xz"],
    },
)

load("@llvm_toolchain//:toolchains.bzl", "llvm_register_toolchains")

llvm_register_toolchains()
