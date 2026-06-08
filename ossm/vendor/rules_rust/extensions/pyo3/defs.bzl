"""# rules_rust_pyo3
"""

load(
    "//private:pyo3.bzl",
    _pyo3_extension = "pyo3_extension",
)
load(
    "//private:pyo3_toolchain.bzl",
    _pyo3_toolchain = "pyo3_toolchain",
    _rust_pyo3_toolchain = "rust_pyo3_toolchain",
)

pyo3_extension = _pyo3_extension
pyo3_toolchain = _pyo3_toolchain
rust_pyo3_toolchain = _rust_pyo3_toolchain
