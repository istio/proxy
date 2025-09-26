# Fixer

This is a simple executable that fills in missing files from plugins that have non-deterministic outputs. This is
particularly useful for gRPC plugins that do not make an output file when the proto file does not contain a service and
also cannot be fixed by using output_directory. As a result, this tool is the option of last resort to fix a plugin that
does not behave well with Bazel.

This executable receives the directory of generated plugin outputs, a file containing the expected file paths and a
path to an empty file template. Any missing files will be filled with the empty file template.

This is implemented in C++, as it is the 'lowest common denominator' language that a user is likely to have working
without relying on another rule set. Python would be another option, but is slightly more likely to have issues.
