
config = struct(
  build_python_zip_default = False,
  supports_whl_extraction = False,
  enable_pystar = True,
  enable_pipstar = True,
  enable_deprecation_warnings = False,
  bazel_8_or_later = False,
  bazel_9_or_later = False,
  bazel_10_or_later = False,
  BuiltinPyInfo = getattr(getattr(native, "legacy_globals", None), "PyInfo", PyInfo),
  BuiltinPyRuntimeInfo = getattr(getattr(native, "legacy_globals", None), "PyRuntimeInfo", PyRuntimeInfo),
  BuiltinPyCcLinkParamsProvider = getattr(getattr(native, "legacy_globals", None), "PyCcLinkParamsProvider", PyCcLinkParamsProvider),
)
