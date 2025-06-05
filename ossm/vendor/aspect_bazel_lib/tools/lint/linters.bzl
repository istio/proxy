"Create linter aspects, see https://github.com/aspect-build/rules_lint/blob/main/docs/linting.md#installation"

load("@aspect_rules_lint//lint:shellcheck.bzl", "lint_shellcheck_aspect")
load("@aspect_rules_lint//lint:vale.bzl", "lint_vale_aspect")

shellcheck = lint_shellcheck_aspect(
    binary = "@multitool//tools/shellcheck",
    config = "@@//:.shellcheckrc",
)

vale = lint_vale_aspect(
    binary = "@@//tools/lint:vale_bin",
    config = "@@//:.vale_ini",
    styles = "@@//tools/lint:vale_styles",
)
