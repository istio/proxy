# rules_license

CI: [![Build status](https://badge.buildkite.com/e12f23186aa579f1e20fcb612a22cd799239c3134bc38e1aff.svg)](https://buildkite.com/bazel/rules-license)

This repository contains a set of rules and tools for
- declaring metadata about packages, such as
  - the licenses the package is available under
  - the canonical package name and version
  - copyright information
  - ... and more TBD in the future
- gathering license declarations into artifacts to ship with code
- applying organization specific compliance constriants against the
  set of packages used by a target.
- producing SBOMs for built artifacts.

WARNING: The code here is still in active initial development and will churn a lot.

## Contact

If you want to follow along:
- Mailing list: [bazel-ssc@bazel.build](https://groups.google.com/a/bazel.build/g/bazel-ssc)  
- Monthly eng meeting: [calendar link](MjAyMjA4MjJUMTYwMDAwWiBjXzUzcHBwZzFudWthZXRmb3E5NzhxaXViNmxzQGc&tmsrc=c_53pppg1nukaetfoq978qiub6ls%40group.calendar.google.com&scp=ALL)
- [Latest docs](https://bazelbuild.github.io/rules_license/latest.html)

## Roadmap

*Last update: October 22, 2023*

### Q4 2023

- Reference implementation for "packages used" tool
  - produce JSON output usable for SBOM generation or other compliance reporting.
- Reference implementation for an SPDX SBOMM generator
  - Support for reading bzlmod lock file
  - Support for reading maven lock file
- "How To" guides
  - produce a license audit
  - produce an SBOM

### Q1 2024

- Add support for other package manager lock file formats
  - ? Python
  - Golang
  - NodeJS
- More SPDX SBOM fields
  - support for including vendor SBOMs
  - 

### Beyond

- Performance improvements
- Sub-SBOMs for tools


- TBD

## Background reading:

These is for learning about the problem space, and our approach to solutions. Concrete specifications will always appear in checked in code rather than documents.
- [License Checking with Bazel](https://docs.google.com/document/d/1uwBuhAoBNrw8tmFs-NxlssI6VRolidGYdYqagLqHWt8/edit#).
- [OSS Licenses and Bazel Dependency Management](https://docs.google.com/document/d/1oY53dQ0pOPEbEvIvQ3TvHcFKClkimlF9AtN89EPiVJU/edit#)
- [Adding OSS license declarations to Bazel](https://docs.google.com/document/d/1XszGbpMYNHk_FGRxKJ9IXW10KxMPdQpF5wWbZFpA4C8/edit#heading=h.5mcn15i0e1ch)
