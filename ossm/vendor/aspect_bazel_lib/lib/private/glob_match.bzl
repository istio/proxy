"""
Basic glob match implementation for starlark based on the golang [doublestar](https://github.com/bmatcuk/doublestar/blob/465a339d8daa03b8620e49b8ae541f71651426ad/match.go#L74) library.

This was originally developed by @jbedard for use in rules_js
(https://github.com/aspect-build/rules_js/blob/6ca32d5199ddc0bf19bd704f591030dc1468ca5f/npm/private/pkg_glob.bzl)
to support the pnpm public-hoist-expr option (https://pnpm.io/npmrc#public-hoist-expr). The pnpm
implementation and tests were used as a reference implementation:
    https://github.com/pnpm/pnpm/blob/v7.4.0-2/packages/matcher/src/index.ts
    https://github.com/pnpm/pnpm/blob/v7.4.0-2/packages/matcher/test/index.ts
"""

# "forever" (2^30) for ~ while(true) loops
_FOREVER = range(1073741824)

def _validate_glob(expr):
    expr_len = len(expr)
    for i in range(expr_len):
        if expr[i] == "*" and i < expr_len - 1 and expr[i + 1] == "*":
            if i > 0 and expr[i - 1] != "/":
                msg = "glob_match: `**` globstar in expression `{}` must be at the start of the expression or preceded by `/`".format(expr)
                fail(msg)
            if i < expr_len - 2 and expr[i + 2] != "/":
                msg = "glob_match: `**` globstar in expression `{}` must be at the end of the expression or followed by `/`".format(expr)
                fail(msg)

def is_glob(expr):
    """Determine if the passed string is a global expression

    Args:
        expr: the potential glob expression

    Returns:
        True if the passed string is a global expression
    """

    return expr.find("*") != -1 or expr.find("?") != -1

def glob_match(expr, path, match_path_separator = False):
    """Test if the passed path matches the glob expression.

    `*` A single asterisk stands for zero or more arbitrary characters except for the the path separator `/` if `match_path_separator` is False

    `?` The question mark stands for exactly one character except for the the path separator `/` if `match_path_separator` is False

    `**` A double asterisk stands for an arbitrary sequence of 0 or more characters. It is only allowed when preceded by either the beginning of the string or a slash. Likewise it must be followed by a slash or the end of the pattern.

    Args:
        expr: the glob expression
        path: the path against which to match the glob expression
        match_path_separator: whether or not to match the path separator '/' when matching `*` and `?` expressions

    Returns:
        True if the path matches the glob expression
    """

    # See https://github.com/bmatcuk/doublestar/blob/465a339d8daa03b8620e49b8ae541f71651426ad/match.go#L74
    # for reference implementation.

    if expr == "":
        fail("glob_match: invalid empty glob expression")

    if expr == "**":
        # matches everything
        return True

    if not is_glob(expr):
        # the expression is not a glob (does bot have any glob symbols) so the only match is an exact match
        return expr == path

    _validate_glob(expr)

    # Cursor of the latest '**' expression within the path
    doublestar_expr_backtrack = -1
    doublestar_path_backtrack = -1

    # Cursor of the latest '*' expression within the path
    star_expr_backtrack = -1
    star_path_backtrack = -1

    # Current indexes into path and expression
    expr_i = 0
    expr_len = len(expr)
    path_i = 0
    path_len = len(path)

    start_of_segment = True

    for _ in _FOREVER:
        if path_i >= path_len:
            break

        # Potentially advance the expression
        if expr_i < expr_len:
            # star
            if expr[expr_i] == "*":
                # Advance past the *
                expr_i = expr_i + 1

                # doublestar
                if expr_i < expr_len and expr[expr_i] == "*":
                    # Assert unsupported ** expressions were prevented by _validate_glob()
                    if not start_of_segment or (expr_i + 1 < expr_len and expr[expr_i + 1] != "/"):
                        fail("glob_match: invalid '**' should be prevented by _validate_glob()")

                    # Advance past the **
                    expr_i = expr_i + 1

                    # Trailing /** matches everything
                    if expr_i >= expr_len:
                        return True

                    # Advance past the **/
                    expr_i = expr_i + 1

                    # Start the doublestar cursor
                    doublestar_expr_backtrack = expr_i
                    doublestar_path_backtrack = path_i
                    star_expr_backtrack = -1
                    star_path_backtrack = -1
                    continue
                else:
                    # Start the star expression cursor
                    start_of_segment = False
                    star_expr_backtrack = expr_i
                    star_path_backtrack = path_i
                    continue

            elif expr[expr_i] == "?":
                start_of_segment = False
                if match_path_separator or path[path_i] != "/":
                    expr_i = expr_i + 1
                    path_i = path_i + 1
                    continue
                else:
                    break

            elif path_i < path_len and expr[expr_i] == path[path_i]:
                start_of_segment = path[path_i] == "/"
                expr_i = expr_i + 1
                path_i = path_i + 1
                continue

        # Did not advance the expression or path.
        # Advance any star expression if possible.
        if star_expr_backtrack >= 0 and (match_path_separator or path[star_path_backtrack] != "/"):
            star_path_backtrack = star_path_backtrack + 1
            expr_i = star_expr_backtrack
            path_i = star_path_backtrack
            start_of_segment = False
            continue

        # Advance any double star expression if possible.
        if doublestar_expr_backtrack >= 0:
            super_continue = False

            # ** backtrack, advance path_i past next separator
            path_i = doublestar_path_backtrack
            for _ in _FOREVER:
                if path_i >= path_len:
                    break

                path_current = path[path_i]
                path_i = path_i + 1

                if path_current == "/":
                    doublestar_path_backtrack = path_i
                    expr_i = doublestar_expr_backtrack
                    start_of_segment = True
                    super_continue = True
                    break

            # Successfully consumed a path segment
            if super_continue:
                continue

        # Failed to advance the path or expression
        return False

    # Exited the loop without reaching the end
    if path_i < path_len:
        return False

    # Reached the end of the path, check if the expression ended or is on a final wildcard
    trailing = expr[expr_i:]
    return trailing == "" or trailing == "*" or trailing == "**"
