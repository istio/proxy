"Public API for expanding variables"

# buildifier: disable=deprecated-function
load("//lib/private:expand_locations.bzl", _expand_locations = "expand_locations")
load("//lib/private:expand_variables.bzl", _expand_variables = "expand_variables")

expand_locations = _expand_locations
expand_variables = _expand_variables
