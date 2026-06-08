rpmbuild_docker
===============

This provides an easy way to create an almalinux-based container that can be used
to run `rpmbuild` via `pkg_rpm()` rules.

For example:

```console
[rules_pkg]$ cd examples/rpm/system_rpmbuild_bzlmod/
[system_rpmbuild_bzlmod]$ ../../../contrib/alma9_rpmbuild_docker/run.sh
[+] Building 0.1s (9/9) FINISHED
```

```console
[devuser@fd4cc85e6e5a system_rpmbuild_bzlmod]$ bazel build test-rpm
Starting local Bazel server (8.3.1) and connecting to it...
INFO: Analyzed target //:test-rpm (80 packages loaded, 3318 targets configured).
INFO: Found 1 target...
Target //:test-rpm up-to-date:
  bazel-bin/test-rpm.rpm
  bazel-bin/test-rpm-all.rpm
  bazel-bin/test-rpm-1-0.all.rpm
INFO: Elapsed time: 5.007s, Critical Path: 0.06s
INFO: 1 process: 9 action cache hit, 1 internal.
INFO: Build completed successfully, 1 total action
```

```console
[devuser@fd4cc85e6e5a system_rpmbuild_bzlmod]$ rpm -qlp bazel-bin/test-rpm.rpm
/BUILD
/MODULE.bazel
/README.md
/test_rpm.spec
```
