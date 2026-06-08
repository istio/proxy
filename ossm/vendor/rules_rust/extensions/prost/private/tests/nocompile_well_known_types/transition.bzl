"""A module defining a transition to disable the `compile_well_known_types` prost toolchain flag"""

load("@rules_rust//rust:rust_common.bzl", "CrateGroupInfo")

def _nocompile_wkt_transition_impl(_settings, _attr):
    return {
        "//private:compile_well_known_types": False,
    }

nocompile_wkt_transition = transition(
    implementation = _nocompile_wkt_transition_impl,
    inputs = [],
    outputs = ["//private:compile_well_known_types"],
)

def _nocompile_wkt_transitioned_impl(ctx):
    return [
        ctx.attr.lib[0][CrateGroupInfo],
    ]

nocompile_wkt_transitioned = rule(
    implementation = _nocompile_wkt_transitioned_impl,
    attrs = {
        "lib": attr.label(
            cfg = nocompile_wkt_transition,
            providers = [CrateGroupInfo],
        ),
    },
)
