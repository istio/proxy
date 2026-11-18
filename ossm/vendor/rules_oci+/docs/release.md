<!-- TODO: remove this doc once https://github.com/bazel-contrib/rules_oci/issues/186 is resolved -->

# Releasing rules_oci

Due to https://github.com/bazel-contrib/rules_oci/issues/186 rules_oci requires manual testing on Darwin before every release.
Since rules_oci is not docker specific, we want to get maximum coverage for all the runtimes out there. In order to do that
rules_oci ought to be tested against runtimes such as podman to diversify the coverage before every release for now.

## Running tests for `.` and `e2e` directories

Just copy-paste this command into the terminal to run the tests

```bash
for dir in "." "e2e/smoke" "e2e/platforms" "e2e/pull"; do
    (echo "## $dir"; cd "$dir" && bazel test //...)
done
```
