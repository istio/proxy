_SYSROOT_BUILD = """
filegroup(
    name = {name},
    srcs = glob(["include/**/*", "lib/**/*", "share/**/*"], allow_empty=True),
    visibility = ["//visibility:public"],
)
"""

_WASI_SDK_ABIS = [
    "wasm32-wasi",
    "wasm32-wasip1",
    "wasm32-wasip1-threads",
    "wasm32-wasip2",
    "wasm32-wasi-threads",
]

def _wasi_sdk_sysroots(ctx):
    ctx.download_and_extract(
        integrity = "sha256-NRcvfSeZSFsVpGsdh/UKWF2RXsZiCA8AXZkVOlCIjwg=",
        stripPrefix = "wasi-sysroot-24.0",
        url = ["https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/wasi-sysroot-24.0.tar.gz"],
    )

    ctx.file("empty/BUILD.bazel", _SYSROOT_BUILD.format(
        name = repr("empty"),
    ))

    for abi in _WASI_SDK_ABIS:
        ctx.file("%s/BUILD.bazel" % (abi,), _SYSROOT_BUILD.format(
            name = repr(abi),
        ))
        ctx.execute(["mv", "include/" + abi, "%s/include" % (abi,)])
        ctx.execute(["mv", "share/" + abi, "%s/share" % (abi,)])

        # This is needed for wasm*-unknown-unknown targets
        ctx.execute(["cp", "-R", "lib/" + abi, "%s/lib" % (abi,)])

        # This is needed for wasm*-wasip1 targets
        ctx.execute(["mv", "lib/" + abi, "%s/lib/%s" % (abi, abi)])

wasi_sdk_sysroots = repository_rule(_wasi_sdk_sysroots)

def _libclang_rt_wasm32(ctx):
    ctx.file("BUILD.bazel", """
exports_files(glob(["*.a"]))
""")

    ctx.download_and_extract(
        integrity = "sha256-fjPA33WLkEabHePKFY4tCn9xk01YhFJbpqNy3gs7Dsc=",
        stripPrefix = "libclang_rt.builtins-wasm32-wasi-24.0",
        url = ["https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/libclang_rt.builtins-wasm32-wasi-24.0.tar.gz"],
    )

libclang_rt_wasm32 = repository_rule(_libclang_rt_wasm32)
