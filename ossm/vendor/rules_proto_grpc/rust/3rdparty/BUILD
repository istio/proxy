# Third party crates/deps for rules_proto_grpc_rust.

load("@rules_rust//crate_universe:defs.bzl", "crate", "crates_vendor")

crates_vendor(
    name = "crates_vendor",
    annotations = {
        "prost-build": [crate.annotation(
            build_script_env = {
                "PROTOC_NO_VENDOR": "true",
                "PROTOC": "$(execpath @com_google_protobuf//:protoc)",
                "PROTOC_INCLUDE": ".",
            },
            build_script_tools = ["@com_google_protobuf//:protoc"],
        )],
    },
    generate_binaries = True,
    mode = "remote",
    packages = {
        "protoc-gen-prost": crate.spec(
            version = "0.2.1",
        ),
        # See https://github.com/neoeinstein/protoc-gen-prost/issues/62
        "protoc-gen-prost-crate": crate.spec(
            git = "https://github.com/neoeinstein/protoc-gen-prost",
            rev = "038cd342677dfa869f8c3a2a2787a139fc561df8",
        ),
        "protoc-gen-tonic": crate.spec(
            version = "0.2.1",
        ),
        "protoc-gen-prost-serde": crate.spec(
            version = "0.2.1",
        ),
        "prost": crate.spec(
            version = "0.11.6",
        ),
        "prost-derive": crate.spec(
            version = "0.11.6",
        ),
        "prost-types": crate.spec(
            version = "0.11.6",
        ),
        "tonic": crate.spec(
            version = "0.8.2",
        ),
        "tonic-build": crate.spec(
            version = "0.8.2",
        ),

        # routeguide example deps
        "serde": crate.spec(
            features = ["derive"],
            version = "1.0.152",
        ),
        "serde_json": crate.spec(
            version = "1.0.91",
        ),
        "rand": crate.spec(
            version = "0.8.5",
        ),
        "tokio": crate.spec(
            version = "1.24.2",
        ),
        "tokio-stream": crate.spec(
            version = "0.1.11",
        ),
        "async-stream": crate.spec(
            version = "0.3.3",
        ),
        "futures": crate.spec(
            version = "0.3.25",
        ),
        # See: https://github.com/influxdata/pbjson/pull/90
        "pbjson-types": crate.spec(
            git = "https://github.com/echochamber/pbjson",
            rev = "a4f760e5d53086d989f28dc9d1d0adcc22fe37c4",
        ),
        "pbjson": crate.spec(
            version = "0.5.1",
        ),
    },
    repository_name = "rules_proto_grpc_rust",
    supported_platform_triples = [
        "i686-apple-darwin",
        "i686-pc-windows-msvc",
        "i686-unknown-linux-gnu",
        "x86_64-apple-darwin",
        "x86_64-pc-windows-msvc",
        "x86_64-unknown-linux-gnu",
        "aarch64-apple-darwin",
        "aarch64-apple-ios",
        "aarch64-apple-ios-sim",
        "aarch64-linux-android",
        "aarch64-unknown-linux-gnu",
        "arm-unknown-linux-gnueabi",
        "armv7-unknown-linux-gnueabi",
        "armv7-linux-androideabi",
        "i686-linux-android",
        "i686-unknown-freebsd",
        "powerpc-unknown-linux-gnu",
        "s390x-unknown-linux-gnu",
        "wasm32-unknown-unknown",
        "wasm32-wasi",
        "x86_64-apple-ios",
        "x86_64-linux-android",
        "x86_64-unknown-freebsd",
        # "riscv32imc-unknown-none-elf", # disabled as @platforms doesn't have //cpu:riscv32 in the version embedded in Bazel 5.x
    ],
    tags = ["manual"],
    vendor_path = "crates",
)
