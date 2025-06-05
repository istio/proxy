# Precompiling

Precompiling is compiling Python source files (`.py` files) into byte code
(`.pyc` files) at build time instead of runtime. Doing it at build time can
improve performance by skipping that work at runtime.

Precompiling is disabled by default, so you must enable it using flags or
attributes to use it.

## Overhead of precompiling

While precompiling helps runtime performance, it has two main costs:
1. Increasing the size (count and disk usage) of runfiles. It approximately
   double the count of the runfiles because for every `.py` file, there is also
   a `.pyc` file. Compiled files are generally around the same size as the
   source files, so it approximately doubles the disk usage.
2. Precompiling requires running an extra action at build time. While
   compiling itself isn't that expensive, the overhead can become noticable
   as more files need to be compiled.

## Binary-level opt-in

Binary-level opt-in allows enabling precompiling on a per-target basic. This is
useful for situations such as:

* Globally enabling precompiling in your `.bazelrc` isn't feasible. This may
  be because some targets don't work with precompiling, e.g. because they're too
  big.
* Enabling precompiling for build tools (exec config targets) separately from
  target-config programs.

To use this approach, set the {bzl:attr}`pyc_collection` attribute on the
binaries/tests that should or should not use precompiling. Then change the
{bzl:flag}`--precompile` default.

The default for the {bzl:attr}`pyc_collection` attribute is controlled by the flag
{bzl:obj}`--@rules_python//python/config_settings:precompile`, so you
can use an opt-in or opt-out approach by setting its value:
* targets must opt-out: `--@rules_python//python/config_settings:precompile=enabled`
* targets must opt-in: `--@rules_python//python/config_settings:precompile=disabled`

## Pyc-only builds

A pyc-only build (aka "source less" builds) is when only `.pyc` files are
included; the source `.py` files are not included.

To enable this, set
{bzl:obj}`--@rules_python//python/config_settings:precompile_source_retention=omit_source`
flag on the command line or the {bzl:attr}`precompile_source_retention=omit_source`
attribute on specific targets.

The advantage of pyc-only builds are:
* Fewer total files in a binary.
* Imports _may_ be _slightly_ faster.

The disadvantages are:
* Error messages will be less precise because the precise line and offset
  information isn't in an pyc file.
* pyc files are Python major-version specific.

:::{note}
pyc files are not a form of hiding source code. They are trivial to uncompile,
and uncompiling them can recover almost the original source.
:::

## Advanced precompiler customization

The default implementation of the precompiler is a persistent, multiplexed,
sandbox-aware, cancellation-enabled, json-protocol worker that uses the same
interpreter as the target toolchain. This works well for local builds, but may
not work as well for remote execution builds. To customize the precompiler, two
mechanisms are available:

* The exec tools toolchain allows customizing the precompiler binary used with
  the {bzl:attr}`precompiler` attribute. Arbitrary binaries are supported.
* The execution requirements can be customized using
  `--@rules_python//tools/precompiler:execution_requirements`. This is a list
  flag that can be repeated. Each entry is a key=value that is added to the
  execution requirements of the `PyCompile` action. Note that this flag
  is specific to the rules_python precompiler. If a custom binary is used,
  this flag will have to be propagated from the custom binary using the
  `testing.ExecutionInfo` provider; refer to the `py_interpreter_program` an

The default precompiler implementation is an asynchronous/concurrent
implementation. If you find it has bugs or hangs, please report them. In the
meantime, the flag `--worker_extra_flag=PyCompile=--worker_impl=serial` can
be used to switch to a synchronous/serial implementation that may not perform
as well, but is less likely to have issues.

The `execution_requirements` keys of most relevance are:
* `supports-workers`: 1 or 0, to indicate if a regular persistent worker is
  desired.
* `supports-multiplex-workers`: 1 o 0, to indicate if a multiplexed persistent
  worker is desired.
* `requires-worker-protocol`: json or proto; the rules_python precompiler
  currently only supports json.
* `supports-multiplex-sandboxing`: 1 or 0, to indicate if sanboxing is of the
  worker is supported.
* `supports-worker-cancellation`: 1 or 1, to indicate if requests to the worker
  can be cancelled.

Note that any execution requirements values can be specified in the flag.

## Known issues, caveats, and idiosyncracies

* Precompiling requires Bazel 7+ with the Pystar rule implementation enabled.
* Mixing rules_python PyInfo with Bazel builtin PyInfo will result in pyc files
  being dropped.
* Precompiled files may not be used in certain cases prior to Python 3.11. This
  occurs due to Python adding the directory of the binary's main `.py` file, which
  causes the module to be found in the workspace source directory instead of
  within the binary's runfiles directory (where the pyc files are). This can
  usually be worked around by removing `sys.path[0]` (or otherwise ensuring the
  runfiles directory comes before the repos source directory in `sys.path`).
* The pyc filename does not include the optimization level (e.g.
  `foo.cpython-39.opt-2.pyc`). This works fine (it's all byte code), but also
  means the interpreter `-O` argument can't be used -- doing so will cause the
  interpreter to look for the non-existent `opt-N` named files.
* Targets with the same source files and different exec properites will result
  in action conflicts. This most commonly occurs when a `py_binary` and
  `py_library` have the same source files. To fix, modify both targets so
  they have the same exec properties. If this is difficult because unsupported
  exec groups end up being passed to the Python rules, please file an issue
  to have those exec groups added to the Python rules.
