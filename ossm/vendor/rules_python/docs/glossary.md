# Glossary

{.glossary}

common attributes
: Every rule has a set of common attributes. See Bazel's
  [Common attributes](https://bazel.build/reference/be/common-definitions#common-attributes)
  for a complete listing

in-build runtime
: An in-build runtime is one where the Python runtime, and all its files, are
known to the build system and a Python binary includes all the necessary parts
of the runtime in its runfiles. Such runtimes may be remotely downloaded, part
of your source control, or mapped in from local files by repositories.

The main advantage of in-build runtimes is they ensure you know what Python
runtime will be used, since it's part of the build itself and included in
the resulting binary. The main disadvantage is the additional work it adds to
building. The whole Python runtime is included in a Python binary's runfiles,
which can be a significant number of files.

platform runtime
: A platform runtime is a Python runtime that is assumed to be installed on the
system where a Python binary runs, whereever that may be. For example, using `/usr/bin/python3`
as the interpreter is a platform runtime -- it assumes that, wherever the binary
runs (your local machine, a remote worker, within a container, etc), that path
is available. Such runtimes are _not_ part of a binary's runfiles.

The main advantage of platform runtimes is they are lightweight insofar as
building the binary is concerned. All Bazel has to do is pass along a string
path to the interpreter. The disadvantage is, if you don't control the systems
being run on, you may get different Python installations than expected.

rule callable
: A function that behaves like a rule. This includes, but is not is not
  limited to:
  * Accepts a `name` arg and other {term}`common attributes`.
  * Has no return value (i.e. returns `None`).
  * Creates at least a target named `name`

  There is usually an implicit interface about what attributes and values are
  accepted; refer to the respective API accepting this type.

simple label
: A `str` or `Label` object but not a _direct_ `select` object. These usually
  mean a string manipulation is occuring, which can't be done on `select`
  objects. Such attributes are usually still configurable if an alias is used,
  and a reference to the alias is passed instead.

nonconfigurable
: A nonconfigurable value cannot use `select`. See Bazel's
  [configurable attributes](https://bazel.build/reference/be/common-definitions#configurable-attributes) documentation.

