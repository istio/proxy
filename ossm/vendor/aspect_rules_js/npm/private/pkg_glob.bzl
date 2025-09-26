"""
Basic glob implementation required for the pnpm public-hoist-exp option
(https://pnpm.io/npmrc#public-hoist-exp).

pnpm implementation and tests:
    https://github.com/pnpm/pnpm/blob/v7.4.0-2/packages/matcher/src/index.ts
    https://github.com/pnpm/pnpm/blob/v7.4.0-2/packages/matcher/test/index.ts
"""

def pkg_glob(exp, pkg):
    """Testing if the passed pkg matches the exp.

    Args:
        exp: the glob expression
        pkg: the package name to test

    Returns:
        True if the package matches the glob expression
    """
    if exp.find("**") != -1:
        fail("pkg_glob does not support ** glob expressions")

    if exp.find("{") != -1 or exp.find("}") != -1 or exp.find("|") != -1:
        fail("pkg_glob does not support {} or | glob expressions")

    exp_i = 0
    pkg_i = 0

    # HACK: assume "|" will never be in the expression and use it to
    # split on * while keeping * as an array entry.
    exp_parts = exp.replace("*", "|*|").strip("|").split("|")

    # Locations a * was terminated that can be rolled back to.
    branches = []

    # Loop "forever" (2^30).
    for _ in range(1073741824):
        subpkg = pkg[pkg_i:] if pkg_i < len(pkg) else None
        subexp = exp_parts[exp_i] if exp_i < len(exp_parts) else None

        # A wildcard in the expression and something to consume.
        if subexp == "*" and subpkg != None:
            # The next part of the expression.
            next_pp = exp_parts[exp_i + 1] if exp_i + 1 < len(exp_parts) else None

            # This wildcard is the last and matches everything beyond here.
            if next_pp == None:
                return True

            # If the next part of the expression matches the current subpkg
            # then advance past the wildcard and consume that next expression.
            if subpkg.startswith(next_pp):
                # Persist the alternative of using the wildcard instead of advancing.
                branches.append([exp_i, pkg_i + 1])
                exp_i = exp_i + 1
            else:
                # Otherwise consume the next character.
                pkg_i = pkg_i + 1

        elif subpkg and subexp and subpkg.startswith(subexp):
            # The string matches the current location in the pkg.
            exp_i = exp_i + 1
            pkg_i = pkg_i + len(subexp)
        elif len(branches) > 0:
            # The string does not match, backup to the previous branch.
            [restored_pattern_i, restored_pkg_i] = branches.pop()

            pkg_i = restored_pkg_i
            exp_i = restored_pattern_i
        else:
            # The string does not match, with no branches to rollback to, there is no match.
            return False

        # Reached the end of the expression and package.
        if pkg_i == len(pkg) and exp_i == len(exp_parts):
            return True

    fail("pkg_glob: reached the end of the (in)finite loop")
