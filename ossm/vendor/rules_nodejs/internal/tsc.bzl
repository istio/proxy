"""
TypeScript compiler.
"""

def tsc(name, srcs, tsconfig, **kwargs):
    """
    Run the tsc typescript compiler.

    Args:
        name: the name of the rule
        srcs: .ts files to compile to output .js files
        tsconfig: a tsconfig file
        **kwargs: additional arguments to pass to native.genrule
    """
    outs = [s.replace(".ts", ".js") for s in srcs] + [s.replace(".ts", ".d.ts") for s in srcs]

    native.genrule(
        name = name,
        srcs = srcs + [tsconfig],
        outs = outs,
        cmd = " ".join([
            "$(NODE_PATH)",
            "./$(execpath @npm_typescript)/bin/tsc",
            "-p",
            "$(execpath %s)" % tsconfig,
            "--typeRoots",
            "./$(execpath @npm_types_node)/..",
            "--declaration",
            "--outDir",
            "$(RULEDIR)",
        ]),
        toolchains = ["@node16_toolchains//:resolved_toolchain"],
        tools = [
            "@node16_toolchains//:resolved_toolchain",
            "@npm_typescript",
            "@npm_types_node",
        ],
        **kwargs
    )
