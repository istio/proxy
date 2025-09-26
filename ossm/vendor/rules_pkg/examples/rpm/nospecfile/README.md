# Using system rpmbuild with bzlmod

## Summary

This example builds an RPM using the system `rpmbuild` from a pure bazel
`pkg_rpm()` definition instead of using a separate specfile.

## To use

```
bazel build :*
rpm2cpio bazel-bin/test-rpm.rpm | cpio -ivt
cat bazel-bin/content.txt
```
