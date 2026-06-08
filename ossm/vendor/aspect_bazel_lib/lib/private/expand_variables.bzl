"Helpers to expand make variables"

load("@bazel_skylib//lib:paths.bzl", _spaths = "paths")

def expand_variables(ctx, s, outs = [], inputs = [], attribute_name = "args"):
    """Expand make variables and substitute like genrule does.

    Bazel [pre-defined variables](https://bazel.build/reference/be/make-variables#predefined_variables)
    are expanded however only `$@`, `$(@D)` and `$(RULEDIR)` of
    [pre-defined genrule variables](https://bazel.build/reference/be/make-variables#predefined_genrule_variables)
    are supported.

    This function is the same as ctx.expand_make_variables with the additional
    genrule-like substitutions of:

      - `$<`: The input file if it is a single file. Else triggers a build error.

      - `$@`: The output file if it is a single file. Else triggers a build error.

      - `$(@D)`: The output directory.

        If there is only one file name in outs, this expands to the directory containing that file.

        If there is only one directory in outs, this expands to the single output directory.

        If there are multiple files, this instead expands to the package's root directory in the bin tree,
        even if all generated files belong to the same subdirectory!

      - `$(RULEDIR)`: The output directory of the rule, that is, the directory
        corresponding to the name of the package containing the rule under the bin tree.

      - `$(BUILD_FILE_PATH)`: ctx.build_file_path

      - `$(VERSION_FILE)`: ctx.version_file.path

      - `$(INFO_FILE)`: ctx.info_file.path

      - `$(TARGET)`: ctx.label

      - `$(WORKSPACE)`: ctx.workspace_name

    See https://docs.bazel.build/versions/main/be/general.html#genrule.cmd and
    https://docs.bazel.build/versions/main/be/make-variables.html#predefined_genrule_variables
    for more information of how these special variables are expanded.

    Args:
        ctx: starlark rule context
        s: expression to expand
        outs: declared outputs of the rule, for expanding references to outputs
        inputs: declared inputs of the rule, for expanding references to inputs
        attribute_name: name of the attribute containing the expression. Used for error reporting.

    Returns:
        `s` with the variables expanded
    """
    rule_dir = _spaths.join(
        ctx.bin_dir.path,
        ctx.label.workspace_root,
        ctx.label.package,
    )
    additional_substitutions = {}

    if s.find("$@") != -1 or s.find("$(@)") != -1:
        if len(outs) > 1:
            fail("$@ substitution may only be used with a single out.")
    if len(outs) == 1:
        additional_substitutions["@"] = outs[0].path
        if outs[0].is_directory:
            output_dir = outs[0].path
        else:
            output_dir = outs[0].dirname
    else:
        output_dir = rule_dir

    if s.find("$<") != -1 or s.find("$(<)") != -1:
        if len(inputs) != 1:
            fail("$< substitution may only be used with a single input.")
        additional_substitutions["<"] = inputs[0].path

    additional_substitutions["@D"] = output_dir
    additional_substitutions["RULEDIR"] = rule_dir

    # Add some additional make variable substitutions for common useful values in the context
    additional_substitutions["BUILD_FILE_PATH"] = ctx.build_file_path
    additional_substitutions["VERSION_FILE"] = ctx.version_file.path
    additional_substitutions["INFO_FILE"] = ctx.info_file.path
    additional_substitutions["TARGET"] = "{}//{}:{}".format(
        "@" + ctx.label.workspace_name if ctx.label.workspace_name else "",
        ctx.label.package,
        ctx.label.name,
    )
    additional_substitutions["WORKSPACE"] = ctx.workspace_name

    return ctx.expand_make_variables(attribute_name, s, additional_substitutions)
