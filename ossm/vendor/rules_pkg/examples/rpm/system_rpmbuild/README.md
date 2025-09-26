# Using system rpmbuild

## Summary

This example uses the `find_system_rpmbuild()` macro built into `rules_pkg`
to search for `rpmbuild` in on the local system and use that to drive the
packaging process.

The RPM itself is based on a user provided spec file.

## To use

```
bazel build :*
rpm2cpio bazel-bin/test-rpm.rpm | cpio -ivt
cat bazel-bin/content.txt
```
