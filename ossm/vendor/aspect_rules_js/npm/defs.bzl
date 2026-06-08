"""Rules for fetching and linking npm dependencies and packaging and linking first-party deps
"""

load(
    "//npm/private:npm_package.bzl",
    _npm_package = "npm_package",
    _stamped_package_json = "stamped_package_json",
)
load(
    "//npm/private:npm_link_package.bzl",
    _npm_link_package = "npm_link_package",
)

npm_package = _npm_package
npm_link_package = _npm_link_package
stamped_package_json = _stamped_package_json
