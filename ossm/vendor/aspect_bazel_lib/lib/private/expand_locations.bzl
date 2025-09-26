"Helpers to expand location"

def expand_locations(ctx, input, targets = []):
    """Expand location templates.

    Expands all `$(execpath ...)`, `$(rootpath ...)` and deprecated `$(location ...)` templates in the
    given string by replacing with the expanded path. Expansion only works for labels that point to direct dependencies
    of this rule or that are explicitly listed in the optional argument targets.

    See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_label_variables.

    Use `$(rootpath)` and `$(rootpaths)` to expand labels to the runfiles path that a built binary can use
    to find its dependencies. This path is of the format:
    - `./file`
    - `path/to/file`
    - `../external_repo/path/to/file`

    Use `$(execpath)` and `$(execpaths)` to expand labels to the execroot (where Bazel runs build actions).
    This is of the format:
    - `./file`
    - `path/to/file`
    - `external/external_repo/path/to/file`
    - `<bin_dir>/path/to/file`
    - `<bin_dir>/external/external_repo/path/to/file`

    The deprecated `$(location)` and `$(locations)` expansions returns either the execpath or rootpath depending on the context.

    Args:
      ctx: context
      input: String to be expanded
      targets: List of targets for additional lookup information.

    Returns:
      The expanded path or the original path

    Deprecated:
      Use vanilla `ctx.expand_location(input, targets = targets)` instead
    """

    return ctx.expand_location(input, targets = targets)
