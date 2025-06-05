# Using a prebuilt rpmbuild instead of the system one.

## Summary

This example defines a rpmbuild toolchain in `local` that can be used
by rules_pkg.  This must be copied into place as `local/rpmbuild_binary`
for use by `register_toolchains()`.

The RPM itself is based on a user provided spec file.

## To use

```
cp /usr/bin/rpmbuild local/rpmbuild_binary
bazel build :*
rpm2cpio bazel-bin/test-rpm.rpm | cpio -ivt
cat bazel-bin/content.txt
```
