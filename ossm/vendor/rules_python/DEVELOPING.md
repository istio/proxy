# For Developers

## Updating internal dependencies

1. Modify the `./python/private/pypi/requirements.txt` file and run:
   ```
   bazel run //private:whl_library_requirements.update
   ```
1. Run the following target to update `twine` dependencies:
   ```
   bazel run //private:requirements.update
   ```
1. Bump the coverage dependencies using the script using:
   ```
   bazel run //tools/private/update_deps:update_coverage_deps <VERSION>
   # for example:
   # bazel run //tools/private/update_deps:update_coverage_deps 7.6.1
   ```

## Releasing

Start from a clean checkout at `main`.

Before running through the release it's good to run the build and the tests locally, and make sure CI is passing. You can
also test-drive the commit in an existing Bazel workspace to sanity check functionality.

### Releasing from HEAD

#### Steps
1. [Determine the next semantic version number](#determining-semantic-version)
1. Create a tag and push, e.g. `git tag 0.5.0 upstream/main && git push upstream --tags`
   NOTE: Pushing the tag will trigger release automation.
1. Watch the release automation run on https://github.com/bazelbuild/rules_python/actions
1. Add missing information to the release notes. The automatic release note
   generation only includes commits associated with issues.

#### Determining Semantic Version

**rules_python** is currently using [Zero-based versioning](https://0ver.org/) and thus backwards-incompatible API
changes still come under the minor-version digit. So releases with API changes and new features bump the minor, and
those with only bug fixes and other minor changes bump the patch digit.

To find if there were any features added or incompatible changes made, review
the commit history. This can be done using github by going to the url:
`https://github.com/bazelbuild/rules_python/compare/<VERSION>...main`.

### Patch release with cherry picks

If a patch release from head would contain changes that aren't appropriate for
a patch release, then the patch release needs to be based on the original
release tag and the patch changes cherry-picked into it.

In this example, release `0.37.0` is being patched to create release `0.37.1`.
The fix being included is commit `deadbeef`.

1. `git checkout -b release/0.37 0.37.0`
1. `git push upstream release/0.37`
1. `git cherry-pick -x deadbeef`
1. Fix merge conflicts, if any.
1. `git cherry-pick --continue` (if applicable)
1. `git push upstream`

If multiple commits need to be applied, repeat the `git cherry-pick` step for
each.

Once the release branch is in the desired state, use `git tag` to tag it, as
done with a release from head. Release automation will do the rest.

#### After release creation in Github

1. Announce the release in the #python channel in the Bazel slack (bazelbuild.slack.com).

## Secrets

### PyPI user rules-python

Part of the release process uploads packages to PyPI as the user `rules-python`.
This account is managed by Google; contact rules-python-pyi@google.com if
something needs to be done with the PyPI account.
