# Using system rpmbuild with bzlmod and generating debuginfo

## Summary

This example uses the `find_system_rpmbuild_bzlmod` module extension to help
us register the system rpmbuild as a toolchain in a bzlmod environment.

It configures the system toolchain to be aware of which debuginfo configuration
to use (defaults to "none", the example uses "centos7").

## To use

```
bazel build :*
rpm2cpio bazel-bin/test-rpm.rpm | cpio -ivt
cat bazel-bin/content.txt
```
