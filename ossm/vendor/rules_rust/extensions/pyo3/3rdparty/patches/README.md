# 3rdparty Patches

## [resolve_cross_compile_config_path](./resolve_cross_compile_config_path.patch)

`pyo3-build-config` for the `resolve-config` feature ends up writing the
`OUT_DIR` environment variable into the compiled library for use in other
build scripts. This is going to lead to broke behavior in any sandboxed
build as the `OUT_DIR` will be an absolute path to a sandbox that is most
likely not going to exist when downstream scripts are run (e.g.
`pyo3-ffi`). Instead, `OUT_DIR` is parsed at runtime so a writable
directory can be found.
