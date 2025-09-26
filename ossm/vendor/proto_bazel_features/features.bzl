bazel_features = struct(
  cc = struct(
    protobuf_on_allowlist = False,
  ),
  proto = struct(
    starlark_proto_info = True,
  ),
  globals = struct(
    PackageSpecificationInfo = PackageSpecificationInfo,
    ProtoInfo = getattr(getattr(native, 'legacy_globals', None), 'ProtoInfo', ProtoInfo)
  ),
)
