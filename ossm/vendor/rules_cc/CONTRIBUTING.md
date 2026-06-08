# Contributing guide

Thank you for your interest in improving rules_cc! This guide will walk you
through the expectations and contribution process for this project.

# Expectations

## Sign the Contributor License Agreement

To contribute to this project, you must sign the Contributor License Agreement.
You (or your employer) retain the copyright to your contribution; this simply
gives us permission to use and redistribute your contributions as part of the
project. You generally only need to submit a CLA once, so if you've already
submitted one (even if it was for a different project), you probably don't need
to do it again.

Visit <https://cla.developers.google.com/> to see your current agreements on
file or to sign a new one.

## Follow Community Guidelines

We expect everyone who interacts with this project to follow [Google's Open
Source Community Guidelines](https://opensource.google.com/conduct/).

# Creating a change

## Ensure your solution is complete

Before proposing a change, ensure your solution includes relevant test coverage
and documentation. Additionally, all code is expected to adhere to the following
language-specific style guides:

* [Google C/C++ style guide](https://google.github.io/styleguide/cppguide.html)
* [Bazel .bzl style guide](https://bazel.build/rules/bzl-style)
* [Bazel BUILD style guide](https://bazel.build/build/style-guide)

## Create a pull request

To propose your changes, [create a pull request from your fork](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork).

When you do this, maintainers will automatically be notified of your proposed changes.

## Fix any breakages

An initial set of checks are run by the GitHub actions in this project. If any
failures are relevant to your changes, promptly address them to ensure a
smoother code review experience.

A second set of checks are run after a Google developer begins the
Google-specific import process (when the `import/copybara` check turns green).
These checks are not visible to open source contributors, so please be patient
and reach out if you need assistance identifying next steps.

## Address review feedback

All incoming changes require review, which is done through GitHub's [pull
request review
UI](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/reviewing-changes-in-pull-requests/reviewing-proposed-changes-in-a-pull-request)

After you address any feedback from maintainers, [re-request
review](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/requesting-a-pull-request-review).

## Await merge

After a change is reviewed and approved by a maintainer, it must be separately
approved by two separate Google engineers through Google's code import process.
During this process, additional tests are run and new breakages may be
identified. If a change is approved but not merged, it is likely caught in this
stage of the process.

There are two stages to this:

* import/copybara - When this is yellow, the import process is not yet started.
  When it is green, this means the import process has started.
* feedback/copybara - This check indicates whether or not Google-internal checks
  pass.

When all of Google's internal checks are satisfied,
[Copybara](https://github.com/google/copybara) will merge the change into both
repositories, closing the associated PR. This does not mean your change is
rejected, it's just the mechanism Copybara uses to communicate that the change
has merged.
