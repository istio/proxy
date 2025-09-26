# Releasing rules_buf

rules_buf contains a Bazel module with Bzlmod and WORKSPACE support which is
pushed to the Bazel Central Repository. Proper release process is necessary to
ensure that the Bzlmod module is published to the BCR properly.

1.  **Run the [Release] workflow.**

    Go to the [Release] workflow page and select <q>Run workflow</q>,
    with the desired version tag (e.g. `v1.2.3`).

    <details>

    <summary>What this workflow does</summary>

    This will create a release tag for the latest `main` revision, `v1.2.3`.

    Note that this workflow creates tags directly on GitHub instead of pushing
    tags up, so it will not indirectly trigger automations that trigger on tags.
    The BCR release script is run as a workflow call. Creating the tags manually
    will not trigger this.

    </details>

1.  **Find the draft release.**

    Upon running the previous workflow, a release draft should be created.
    Check for it in the [releases page].

    If for some reason this doesn't happen, check the workflow log for more
    information.

    <details>

    <summary>Manually creating a release draft</summary>

    Note that manually-created releases will not pass attestation and can not
    be pushed to the BCR.

    To manually create a release draft, run `.github/workflows/release_prep.sh`
    with the version tag (e.g. `vX.Y.Z`) as an argument, while checked out to
    the release tag/commit:

    ```
    .github/workflows/release_prep.sh v1.2.3 >release_notes.md
    ```

    This will create two files:

    - `release_notes.md`: This should be prepended to the GitHub-generated
      release notes. It contains instructions on how to include the repo with
      Bazel.
    - `rules_buf-1.2.3.tar.gz`: This should be attached to the release. It
      includes a stable tarball of the release commit for Bazel.

    </details>

1.  **Publish the release.**

    Once the release draft is created, edit it as needed, prepending any
    important notes (e.g. breaking changes), and finally, publish it.

1.  **Check [Bazel Central Registry repository] for a pull request.**

    Shortly after publishing the release, the [Publish to BCR] workflow should
    create a new pull request. There may be failures in CI that need to be
    addressed.

[Release]: https://github.com/bufbuild/rules_buf/actions/workflows/release.yaml
[releases page]: https://github.com/bufbuild/rules_buf/releases
[Bazel Central Registry repository]: https://github.com/bazelbuild/bazel-central-registry/pulls
[Publish to BCR]: https://github.com/bazel-contrib/publish-to-bcr
