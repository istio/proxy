config = struct(
  enable_pystar = True,
  enable_deprecation_warnings = False,
  BuiltinPyInfo = getattr(getattr(native, "legacy_globals", None), "PyInfo", PyInfo),
  BuiltinPyRuntimeInfo = getattr(getattr(native, "legacy_globals", None), "PyRuntimeInfo", PyRuntimeInfo),
  BuiltinPyCcLinkParamsProvider = getattr(getattr(native, "legacy_globals", None), "PyCcLinkParamsProvider", PyCcLinkParamsProvider),
)
