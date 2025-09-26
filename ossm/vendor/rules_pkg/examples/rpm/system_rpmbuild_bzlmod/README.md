# Using system rpmbuild with bzlmod

## Summary

This example uses the `find_system_rpmbuild_bzlmod` module extension to help
us register the system rpmbuild as a toolchain in a bzlmod environment.

The RPM itself is based on a user provided spec file.

## To use

```
bazel build :*
rpm2cpio bazel-bin/test-rpm.rpm | cpio -ivt
cat bazel-bin/content.txt
```
