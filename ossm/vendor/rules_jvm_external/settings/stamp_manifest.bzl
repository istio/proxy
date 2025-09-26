StampManifestProvider = provider(fields = ["stamp_enabled"])

def _impl(ctx):
    return StampManifestProvider(stamp_enabled = ctx.build_setting_value)

stamp_manifest = rule(
    implementation = _impl,
    build_setting = config.bool(flag = True),
)
