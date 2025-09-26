The release process is pretty straightforward:

## Pick a version number

The release number uses a `major.minor` version format, and should
just be numbers. If there's a major feature or a breaking change,
don't be shy about bumping the major number: lots of people have a
strong belief in SemVer and it would be a pity to let that faith down.

## Set up a tracking issue in GitHub

This isn't strictly necessary, but it's polite since it allows us to
coordinate the work that we need to complete for the next release.

## Make sure all dependencies are up to date

Open up `repositories.bzl` and make sure that our dependencies are the
most recent versions. If they're not, create a PR to update them, and
land the change.

## Update the `MODULE.bazel` file to reflect the latest changes

There are three things you need to check

1. The `version` parameter in the `module` declaration must be the
   same as the version number of the release you're about to push.
2. The `bazel_dep` entries must match the entries in
   `repositories.bzl`
3. The deps in the `maven.install` (named `rules_jvm_external_deps`)
   must match the same deps from `repositories.bzl`

You may well need to create a PR to update these too.

## Tag the release in `git` and push the tag

The tag format is just the release number (eg. `10.5`) without any
additional prefixes or suffixes. We use a `major.minor` version
format.

```shell
git switch master
git pull
git tag XX.YY
git push --tags
```

## Create a draft entry in the GitHub releases.

I like to copy the previous release, and then edit it. There's a handy
button in the UI that will generate some release notes. Use it, then
edit the values it gives you. The release notes should include user
visible changes and all commits by people who aren't core
committers. Bug fixes and little tweaks the core committers have made
can be omitted if you don't think they're particularly noteworthy.

Name the draft release after the version you've picked.

## Create an archive of the release, and add as an asset

GitHub periodically break how archives are created. To avoid this,
package the current release, and upload it as an asset to your draft
release.

You can use
`git archive --format=tar --prefix=rules_jvm_external-${TAG}/ ${TAG} | gzip > rules_jvm_external-{TAG}.tar.gz`
to generate the archive.

## Publish the release

Press the magic buttons in the GitHub UI.

## Prepare the upload to the BCR

Clone the [BCR repo][bcr], and prepare an update PR. How do you do
this? I'm glad you asked!

```shell
bazel run //tools:add_module
```

If you've bumped the major version number, you may want to update the
"compatibility level", but only if the change isn't backwards
compatible.

Below is an example run, where the source for this repo is at
`/Volumes/Dev/src/github.com/bazelbuild/rules_jvm_external`

```shell
% bazel run tools:add_module                                                                                                                                       main*
INFO: Invocation ID: aa3e5952-e546-4ba5-a30a-7333c05a47e3
INFO: Analyzed target //tools:add_module (0 packages loaded, 0 targets configured).
INFO: Found 1 target...
Target //tools:add_module up-to-date:
  bazel-bin/tools/add_module
INFO: Elapsed time: 0.076s, Critical Path: 0.00s
INFO: 1 process: 1 internal.
INFO: Build completed successfully, 1 total action
INFO: Running command line: bazel-bin/tools/add_module
INFO: Getting module information from user input...
ACTION: Please enter the module name: rules_jvm_external
ACTION: Please enter the module version: 5.1
ACTION: Please enter the compatibility level [default is 1]: 
ACTION: Please enter the URL of the source archive: https://github.com/bazelbuild/rules_jvm_external/releases/download/5.0/rules_jvm_external-5.1.tar.gz
ACTION: Please enter the strip_prefix value of the archive [default None]: rules_jvm_external-5.1
ACTION: Do you want to add patch files? [y/N]: n
ACTION: Do you want to add a BUILD file? [y/N]: n
ACTION: Do you want to specify a MODULE.bazel file? [y/N]: y
ACTION: Please enter the MODULE.bazel file path: /Volumes/Dev/src/github.com/bazelbuild/rules_jvm_external/MODULE.bazel
ACTION: Do you want to specify an existing presubmit.yml file? (See https://github.com/bazelbuild/bazel-central-registry/tree/main#presubmityml) [y/N]: 
ACTION: Please enter a list of build targets you want to expose to downstream users, separated by `,`: @rules_jvm_external//:implementation,@rules_jvm_external//private/tools/java/...
ACTION: Do you have a test module in your source archive? [Y/n]: y
ACTION: Please enter the test module path in your source archive: examples/bzlmod
ACTION: Please enter a list of build targets for the test module, separated by `,`: //java/src/com/github/rules_jvm_external/examples/bzlmod:bzlmod_example
ACTION: Please enter a list of test targets for the test module, separated by `,`: 
```

Once this is done, the script will generate a PR for you to upload the
to BCR. Once that PR is merged, you're done.

[bcr]: https://github.com/bazelbuild/bazel-central-registry