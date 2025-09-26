# Patch Acceptance Process

- PRs that change or add behavior are not accepted without being tied to an
   issue.  Most fixes, even if you think they are obvious, require an issue
   too. Almost every change breaks someone who has depended on the behavior,
   even broken behavior.
- Significant changes need a design document. Please create an issue describing
   the proposed change, and post a link to it to rules-pkg-discuss@googlegroups.com.
   Wait for discussion to come to agreement before proceeding.
-  Features and bug fixes should be as portable as possible.
   - do not not disable tests on Windows because it is convenient for you
   - if a feature is only available on specific platforms, it must be optional. That
     is, it requires a distinct bzlmod MODULE
-  All fixes and features must have tests.
-  Ensure you've signed a [Contributor License
   Agreement](https://cla.developers.google.com).
-  Send us a pull request on
   [GitHub](https://github.com/bazelbuild/rules_pkg/pulls). If you're new to GitHub,
   read [about pull
   requests](https://help.github.com/articles/about-pull-requests/). Note that
   we restrict permissions to create branches on the main repository, so
   you will need to push your commit to [your own fork of the
   repository](https://help.github.com/articles/working-with-forks/).
-  Wait for a repository owner to assign you a reviewer. We strive to do that
   within 4 business days, but it may take longer. If your review gets lost
   you can escalate by starting a thread on
   [GitHub Discussions](https://github.com/bazelbuild/bazel/discussions).
-  Work with the reviewer to complete a code review. For each change, create a
   new commit and push it to make changes to your pull request.
-  A maintainer will approve the PR and merge it.

Tips
-  Large PRs are harder to review. If you have to refactor code to implement a feature
   please split that into at least 2 PRs. The first to refactor without changing behavior
   and the second to implemtn the new behavior. Of course, as above, any PR that large
   should be discussed in an issue first
-  Please do not send PRs that update dependencies (WORKSPACE or MODULE.bzl) just to
   stay at head.  We try to maintain backwards compatibility to LTS releases as long as
   possible, so we only update to new versions of dependencies when it is required.

For further information about working with Bazel and rules in general:
-  Read the [Bazel governance plan](https://www.bazel.build/governance.html).
-  Read the [contributing to Bazel](https://www.bazel.build/contributing.html) guide.
