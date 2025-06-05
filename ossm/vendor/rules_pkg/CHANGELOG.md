# Release 0.10.1

This is a bug fix release.

**Bug Fixes**
    * Temporarily restore //mappings.bzl and //pkg.bzl  (#817)
    * Fix RPM package release and version files expansion (#816)
    * Apply tar remap_paths to runfiles full paths (#812)
    * Use raw string for docstring that contains a backslash (for Python 3.12) (#815)

Thanks to: Alex Bozhenko, Chuck Grindel, Diego Ortin, and Tomasz Wojno


# Release 0.10.0

**New Features**
-   Allow $(var) substitution in filenames (#620)
-   Rough prototype of @since processing. (#617)
-   First cut at runfiles support in pkg_* rules (#605)
-   Allow substitution of user-defined variables in RPM preamble (#787)
-   Add %posttrans scriptlet to RPM package (#799)
-   Allow additional RPM macro defines (#794)
-   Bring tar runfiles up to feature parity with pkg_files.runfiles. (#754)
-   Add support for `Obsoletes` tag in RPM definition (#778)
-   pkg_deb: allow data.tar.zst (#761)
-   Add support for failing on file conflicts. (#683)
-   Make pkg_zip compression configurable (#737)
-   Append changelog to RPM spec file (#726)
-   Add basic include_runfiles to pkg_files. (#724)
-   Add changelog attribute to pkg_deb (#725)
-   Add support for setting uid/gid from pkg_attributes (#671)

**Bug Fixes**
-   Explicitly set the FILE bit in zip external attributes. (#802)
-   Explicitly set `%{_builddir}` macro (#792)
-   Only inject pre and post scriptlets when provided (#788)
-   Don't load cc toolchain from rules_cc (#779)
-   doc: Fixup external manual references (#777)
-   Get bzlmod working in CI (#766)
-   use runfiles from rules_python (#768)
-   When pkg_tar.prefix_dir == base of symlink path, don't double-dip. (#749)
-   add imports to fix bazel --noexperimental_python_import_all_repositories flag (#630)
-   Align pkg_rpm returned files with other rules (#692)
-   fix(pkg_tar): properly normalize paths for empty files (#760)
-   Document that package_dir also uses package_variables (#747)
-   Fix handling paths with whitespaces (#733)
-   Fix python 3.6, doesn't support compresslevel
-   Use Gzip compress level 6 (#720)
-   write debian Date field in UTC rather than local time (#712)
-   [pkg_deb] Fix multiline fields in changes file (#691)

**Breaking Changes
-   Remove PackageArtifactsInfo. (#752)

Thanks to: Adam Azarchs, Alex Eagle, August Karlstedt, Austin Schuh, Adrian Vogelsgesang,
flode, Florian Scheibner, Ignas Kaziukėnas, Jean-Hadrien Chabran, Matt,
Mike Kelly, Paul Draper, Sam Schwebach, Tomasz Wojno, and Vertexwahn
for contributions to this release.

# Release 0.9.x

**New Features**
-   Add OutputGroupInfo for pkg_rpm rule (#684)
-   Add verify_archive rule to do e2e tests on built archives. (#669)
-   Expose tar manifest as an output (#643)
-   Support license attribute in pkg_deb (#651)
-   Add support for the txz extension in pkg_tar (#653) (#654)

**Bug Fixes**
-   pkg_tar should not prefix tree artifacts with ./ (#681)
-   Fix a potential TypeException caused by None type (#668)
-   pkg_zip: Some unicode file handling fixes and basic tests (#641)
-   pkg_tar, pkg_zip: improve support for long paths on Windows (#672)
-   Explicitly store implicit parent directories in zip files (#640)
-   Remove unnecessary `to_list()` calls (#639)

Thanks to: Clint Harrison Qingyu Sui, Fabian Meumertzheim, Ryan Beasley, Andrew Psaltis
Alex Eagle, Nils Semmelrock, and Doug Rabson
for contributions to this release.


# Release 0.8.0

**New Features**
-   Allow $(var) substitution in filenames (#620)
    * Allow $(var) substitution in filenames and include everything in ctx.var in the substitution dictionary.
    Fixes #20
-   Basic bzlmod setup
    - CI runs for both traditional and bzlmod
    - Shows it working for one example
    - Has only runtime deps
    - rpm and git toolchains not done yet
-   Rough prototype of @since processing. (#617)
-   First cut at runfiles support in pkg_* rules (#605)

**Bug Fixes**
-   Fix config_setting visibility failure when using `--incompatible_config_setting_private_default_visibility`
-   Cosmetic. Improve the error messageing for duplicate files in check_dest. (#616)
-   Adjust tar tests to have a test case for #297 (#618)
-   Do not warn if the origin paths are the same. (#615)

# Release 0.7.0

## New Features
- Make the .bzl files available as input to a bzl_library (#567)
- Allow pkg_files.strip_prefix to work on tree artifact without having to use `renames`.
- Add @rules_license style license declarations (#508)
- Better looking [documentation](https://bazelbuild.github.io/rules_pkg/0.7.0/reference.html)
- Add `artifact_name` to `print_relnotes` (#541)
- pkg_{deb,rpm,tar,zip} are now available via individual .bzl files, so you
  only need to load what you actually need.
- Add tree artifact support to pkg_zip (#537)
- symlink support to pkg_zip (#499)

## Potentially breaking changes

- Get rid of the long deprecated archive_name and extension from pkg_zip. (#552)
  - Make 'out' work in a reasonable way. Fixes #414
  - Partial fix for #284

## Bug fixes:

- Only allow .deb Description to be multiline. (#573)
  Fixes: https://github.com/bazelbuild/rules_pkg/issues/572
- Fix pkg_tar to not add the ./ to the prefix of every member. (#554). Closes: #531
- Stop stripping tree artifact root name in pkg_tar and pkg_zip. (#555). Closes #404
- Fix RPM source date epoch for rpmbuild 4.17+ (#529)


## Contributors

Thanks to: Andrew Psaltis, Gunnar Wagenknecht, and Sven Mueller
for contributions to this release.


# Release 0.6.0

This revision requires Bazel 4.x or greater

## Visible changes
- Enable nested pkg_filegroups (#420)
- Restore `include_runfiles` support for pkg_tar. (#398)
- Change the Debian example to reflect standard Debian naming. (#389)
- More support for TreeArtifacts in PackageFilesInfo (#421)
- Improved docs
  - Provide on https://bazelbuild.github.io/rules_pkg/0.6.0/reference.html
  - Provide an example for using the new packaging rules (#375)
  - Create "where is my output" example (#432)
  - post process docs to work around stardoc bugs.
- eliminate need to load all rules if you only need one.

## Bug fixes:
- [pkg_deb] fix computation of changes file file name (#418)
- Fix use of name parameter to pkg_tar (#469)
- Fix missing `%dir` RPM filetag when `PackageDirsInfo` is provided (#473)
- pkg_rpm: Don't have source_date_epoch apply by default; test modularity cleanup (#487)

## Internal changes
- Remove legacy command line options to tar builder. Everything is now in the manifest.
- Unify template files names as .tpl (#383)
- Modularize manifest python code (#384)
- many code refactorings to make tests more isolated and allow more ownership by domain

## Contributors
Thanks to: Andrew Psaltis, Grant Monroe, Gunnar Wagenknecht, Ken Conley, Motiejus Jakštys, and Ryan Beasley
for contributions to this release.


# Release 0.5.1

## New features
- Add `pkg_deb(architecture_file)` to provide a way to set the Debian package architecture from the content of a file created at build time.  (#390)
- Provide `pkg_install` for a "make install"-like experience in Bazel (#380)
  - Note: This feature is still in development. Read the PR for more information.
- Initial support for the `pkg_filegroup` framework in `pkg_zip` (#373)

## Closed bugs

- Change the Debian example to reflect standard Debian naming. (#389)
- pkg_tar(include_runfiles) now works again (#392)

# Release 0.5.0

## New Features

- Initial support for `pkg_*` rules as srcs of `pkg_tar` (#367)
  Adds support for `pkg_mklink`, `pkg_mkdirs`, `pkg_files` and `pkg_filegroup` to `pkg_tar`.
  - Provide `filter_directory` for basic TreeArtifact processing (#331)
- `stamp` support to `pkg_tar` (#288) and `pkg_zip` (#365)
  Done in the style of cc_binary
  - `stamp` attribute (1=stamp, 0=no stamp, -1=follow `--stamp`)
  - Use existing `--stamp` command line option
- Significant `pkg_rpm` changes
  - Graduate experimental `pkg_rpm` to mainline (#338)
  - Deprecate pkg/rpm.bzl and move it to pkg/legacy/rpm.bzl (#337)
  - Make `find_system_rpmbuild` repo rule depend on `PATH` (#348)
  - Support SOURCE_DATE_EPOCH in rpm.bzl; enable file clamping in make_rpm.py (#322)
  - Allow runfiles to be used alongside the `rpmbuild` toolchain (#329)
- `pkg_tar` support for custom compression program (#320)
- Support long file names in `pkg_tar` by ignoring 'path' PAX header. (#250) (#326)
- (experimental) Capability to gather the git commit log since the last release (#357)
  This needs user feedback to discover the most pleasing mode.  For this
  release, I did `blaze build distro:changelog.txt` then took the result to
  update this file (CHANGELOG.md).  I would like to do better than that. Thoughts
  from users are welcome.

## Internal changes

- Add a rule to make artifacts that mimic binaries (#366)
- Stop importing single methods from modules. Use the entire thing. (#361)
- Fix some things that Google's strident buildifier warns about. (#363)
- Enable more tests on windows (#364)
  - Fix most mapping tests to work on windows. (#350)
- refactor `path.bzl` and improve testing (#359)
- Fix and refactor pkg_tar compression logic (#358) (#345)
- Move `build_tar`, `build_zip` and helpers to //private (#353)

## Incompatible changes

- Remove the capability to have the Debian .changes file have a different (#342)

## Contributors

This release contains contributions and fixes from aiuto, Alex Eagle,
Andrew Psaltis, Greg Bowyer, katre, Michael Hackner, and Rafael Marinheiro

# Release 0.4.0

## New Features

- `package_file_name` & `package_variables` to allow dynamically named output files.
- `rpmbuild` is now a toolchain allowing you better control using your own vs. the system one
- Portions of the `pkg_filegroup` rule suite are available in @rules_pkg//:mappings.bzl, but there are no packaging rules that use it at this time. Rules that use it will be added in 1.0

## Incompatible Changes
- `archive_name` is now deprecated. To be removed before 1.0 `WORKSPACE` setup

## Contributors
This release contains contributions and fixes from Andrew Psaltis, dmayle, Konstantin Erman, Martin Medler, Motiejus Jakštys, Thi Doãn, Thomas Gish, and Xavier Bonaventura.

# Release 0.3.0

This release features contributions by the Bazel team and
andreas-0815-qwertz, Andrew Psaltis, Daniel Sullivan, David Schneider,
Elliot Murphy, Matthias Frei, Matt Mikitka, Pras Velagapudi, Shimin Guo,
and Ulf Adams

**New Features**
-   commit a4296dac48144dd839da1f093dce00fd3078eaf5

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Thu Oct 1 17:55:20 2020 -0400
    experimental/rpm.bzl: Make compression settings configurable (#240)
    This change adds a new attribute binary_payload_compression to the
    experimental pkg_rpm rule. It is implemented in terms of command-line options
    to rpmbuild(8), which are passed in via make_rpm.py.
    We do this by instructing rpmbuild to define the _binary_payload macro on its command line. Tests are also provided.
    This can also be added to the non-experimental pkg_rpm rule in the future; the
    code would be similar.
    Fixes #239.
-   commit aa18e176673d03f05f1d3f0537bae424cfc225fe

-   Author: aiuto <aiuto@google.com>
    Date:   Tue Sep 29 22:32:00 2020 -0400
    Provide capability to rename archives based on configuration values (#198)
    TODO for later
    - create cookbook style examples
    - emit PkgFilegroupInfo when that is available
    - implement for pkg_deb, rpm, zip
-   commit 55a1a9b2eca5b78a44fb940f3143a1d88423c2e2

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Thu Sep 24 15:16:00 2020 -0400
    Use symlink actions for RPM packages instead of custom actions (#236)
    Like #232, but in the RPM builders.  No other callouts to `ln(1)` are made
    directly by `rules_pkg`.
-   commit effac40a51fea25bd566f266f6b7b1d23327f02a

-   Author: aiuto <aiuto@google.com>
    Date:   Thu Sep 24 08:16:35 2020 -0400
    Add LICENSE to distribution tarball (#233)
    Fixes #231
-   commit bd9c7712163c03133b086e4efa0e19ed802aa956

-   Author: Ulf Adams <ulfjack@users.noreply.github.com>
    Date:   Thu Sep 24 03:35:39 2020 +0200
    Use a symlink action, not a shell script (#232)
    Using a shell script is not portable and also causes issues with
    build-without-the-bytes, which can handle local symlink actions (at least in
    principle), but cannot handle symlinks returned from remote execution.
    Also see https://github.com/bazelbuild/bazel/issues/11532.
-   commit 60fbda7768d16e0c5c87f9efdc775f0586001657

-   Author: Daniel Sullivan <danielalexandersullivan@gmail.com>
    Date:   Thu Sep 10 10:00:48 2020 -0400
    Avoid stripping prefixes with incomplete directory names.
-   commit e0d807d0ec4edfc5370f87259ae3461f86f0edb5

-   Author: David Schneider <dschneider@tableau.com>
    Date:   Tue Sep 8 18:02:30 2020 -0700
    Add attribute strip_prefix to pkg_zip (#221) (#230)
-   commit 2ad5bd8fa4dc034081c616521bc7d6a45eb21818

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Wed Aug 26 23:02:56 2020 -0400
    `pkg_rpm_ex`: Provide in-rule dependency specifications (#224)
    This change provides four additional attributes to the experimental `pkg_rpm`
    rule, namely:
    - `conflicts`, `string_list`, corresponding the `Conflicts` tag
    - `provides`, `string_list`, corresponding the `Provides` tag
    - `requires`, `string_list`, corresponding the `Requires` tag
    Additionally:
    - `requires_contextual`, `string_list_dict`, providing the capability to specify
    tags like `Requires(postun)`.
    Unit tests were also provided.  Non-rpm input files for the `pkg_rpm_ex` output
    test have now been given the ".csv" extension to better help identify that they
    are delimited files (but not strictly comma-delimited).
    Note that these changes only impact the "strong" dependencies between packages;
    [weak dependencies] like "Suggests" and "Recommends" are not explicitly
    supported, but can be added in the future following a similar pattern.
    Fixes #223.
    [weak dependencies]: https://rpm.org/user_doc/dependencies.html#weak-dependencies
-   commit 30bb5b27d11d117259a41d9356ddce0272932d60

-   Author: Shimin Guo <shimin.guo@mixpanel.com>
    Date:   Wed Aug 26 19:30:29 2020 -0700
    Support "provides" in pkg_deb (#225)
-   commit 569a0e57e98e96f7075f500fcc76609d68b465e2

-   Author: dannysullivan <danielalexandersullivan@gmail.com>
    Date:   Mon Aug 24 18:23:12 2020 -0400
    Avoid stripping prefixes with incomplete directory names.
-   commit f7cec56170384450c06004c37520b72e22952999

-   Author: Daniel Sullivan <danielalexandersullivan@gmail.com>
    Date:   Thu Aug 20 10:44:46 2020 -0400
    Remove support for Python 2 (#222)
-   commit 4ad8aa08e02694b71bfc0713da4793150aa4f4bc

-   Author: Tony Aiuto <aiuto@google.com>
    Date:   Fri Aug 7 11:46:28 2020 -0400
    Remove testenv.sh. It is no longer used.
-   commit e1b3bea742bc9b2b9d4c7862deafe82a749386cf

-   Author: aiuto <aiuto@google.com>
    Date:   Fri Aug 7 11:33:01 2020 -0400
    use runfiles for pkg_tar_test to enable on Windows (#215)
    This is a precursor to getting pkg_tar_test to run on Windows.
    There are still problems to resolve.
-   commit 933fa6fc7fc49788af04a60558722180818d091f

-   Author: aiuto <aiuto@google.com>
    Date:   Fri Aug 7 11:22:20 2020 -0400
    Convert pkg_deb tests from shell to python (#211)
-   commit dbd2c4b1f3186703c0b933429fc03924cafb8e4c

-   Author: aiuto <aiuto@google.com>
    Date:   Fri Aug 7 11:17:32 2020 -0400
    write debian control tarball in GNU_FORMAT. (#217)
    This addresses part 1 of #216
-   commit 5985cfff16486f52bb029edd2732ad5868622b94

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Mon Aug 3 23:26:00 2020 -0400
    Consolidate make_rpm.bzl and experimental/make_rpm.bzl (#179)
    * Consolidate make_rpm.bzl and experimental/make_rpm.bzl
    This change consolidates all of new features/fixes from
    pkg/experimental/make_rpm.bzl into pkg/make_rpm.bzl.
    Code used specifically by experimental/rpm.bzl is marked as such, and is grouped
    as to be generally obvious.
    Features now supported by non-experimental `pkg_rpm`:
    - Support for older versions of `rpm(8)` (creation of `RPMS` output directory)
    Features supported by non-experimental `pkg_rpm` not supported by experimental
    `pkg_rpm`:
    - Support for `SOURCE_DATE_EPOCH`
    Fixes #173
-   commit 4b0b9f4679484f107f750a60190ff5ec6b164a5f

-   Author: aiuto <aiuto@google.com>
    Date:   Wed Jul 15 14:59:02 2020 -0400
    Add windows to presubmit tests. (#206)
    * add windows to CI. Not everything, but enough to get started
-   commit dede700a2f4aea9854238cad1833b54585f65d43

-   Author: aiuto <aiuto@google.com>
    Date:   Wed Jul 15 12:47:59 2020 -0400
    Drop dependency on xzcat (#205)
    We simply switch to use tarfile mode 'r:*' so that the Python runtime can pick the correct decompression.
    This simplifies the code enormously.
    The removed lines expressed concerns about the performance of the python3 implementation of xz decompression.
    Those comments are fairly old, and were addressed in recent Python implementations as noted here: https://bugs.python.org/issue18003
-   commit 67d64ba774b3e4c33061cf7a95215112c65657a9

-   Author: aiuto <aiuto@google.com>
    Date:   Wed Jul 15 12:21:48 2020 -0400
    Convert build_tar_test to python for readability and portability (#196)
-   commit 154479d9284535c77a781b5be26354f6d46cfe63

-   Author: Elliot Murphy <statik@users.noreply.github.com>
    Date:   Thu Jul 9 16:01:27 2020 -0400
    Control mode on zip inputs (#96) (#97)
-   commit f00b356970524f53d928503ca5443c6108e883e5


-   Author: David Schneider <dschneider@tableau.com>
    Date:   Wed Jul 1 07:28:00 2020 -0700
    Allow custom archive name for pkg_tar, pkg_deb, and pkg_zip targets (#194)
-   commit b5add5bf465c626407ad0ded679a0e2c14dad801

-   Author: aiuto <aiuto@google.com>
    Date:   Thu May 28 10:42:00 2020 -0400
    Add licenses clauses to BUILD files (#186)
-   commit 2977b089a6cd45038d39a5498f0a6cabb11774bf

-   Author: aiuto <aiuto@google.com>
    Date:   Fri May 22 14:50:42 2020 -0400
    Add a minimal WORKSPACE file to the distribution. (#182)
    We extract this from the top level workspace so it is easier to keep in sync.
-   commit 808c192a0c48f292e6dfaaeb3bfa3d4378f6996d

-   Author: andreas-0815-qwertz <57450822+andreas-0815-qwertz@users.noreply.github.com>
    Date:   Tue May 19 18:51:17 2020 +0200
    Make sequence of filenames in control.tar predictable (#120)
    Replacing plain Python dict by an OrderedDict for Python versions
    before 3.7, so that iteration order of "extrafiles" is determined by
    insertion order.  Since 3.7 iteration order of plain dict itself is
    stable.
    https://github.com/bazelbuild/rules_pkg/issues/114
-   commit eab297c6931260e283a3daaf2cdc438f0c6a1cfc

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Mon May 18 22:40:49 2020 -0400
    Fix `pkg_rpm` `source_date_epoch` attribute (#176)
    The existing `source_date_epoch` attribute for `pkg_rpm` is passed in as
    an `int`, and is then appended to a string.  This, naturally, fails.
    The `int` needs to be stringified first.
    Additionally, since the [SOURCE_DATE_EPOCH] value can be any valid UNIX
    epoch value, account for the possibility of it being set to 0 in the
    Starlark code.
    [SOURCE_DATE_EPOCH]: https://reproducible-builds.org/specs/source-date-epoch/#idm55
-   commit ec802488edc1a0594b019fadf1794f13b434b365

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Fri Apr 10 10:33:50 2020 -0400
    Provide pkg_mklinks for creation of in-package symbolic links
    Symbolic links are often included within packages, referring to files and
    directories within and without.
    This commit provides a rule, `pkg_mklinks`, which allows for the creation of
    arbitrary symbolic links within a package, and support within the experimental
    `pkg_rpm` rule to emit them.
    `buildifier` was also opportunistically applied to files in this change.
-   commit b20c45f292be6c74d2f0d829ba02c83dbe271195

# Release 0.2.6

**New Features**

-   Author: aiuto <aiuto@google.com>
    Date:   Mon Apr 27 15:47:20 2020 -0400
    Add support to generate stardoc. (#164)
    This is done in a manner so there is no new runtime dependency on bazel-skylib. The bzl_library needed as input to stardoc is only created within the distro directory, which is not part of the released package.
-   commit eea3f696ca3187897ddc3eb07d9955384809a84c

-   Merge: 0761c40 b4c4a91
    Author: Daniel Sullivan <danielalexandersullivan@gmail.com>
    Date:   Fri Apr 24 11:02:10 2020 -0400
    Merge pull request #162 from aiuto/lic
    remove useless BUILD file.  update readme
-   commit b4c4a91dc36a53bb5e9e1fc42c91f351d782a7ed

-   Author: Tony Aiuto <aiuto@google.com>
    Date:   Thu Apr 23 23:53:56 2020 -0400
    remove useless BUILD file.  update readme
-   commit 0761c40f7f1d265ebd814a11eaa03e327271ae5c

-   Author: Daniel Sullivan <danielalexandersullivan@gmail.com>
    Date:   Mon Apr 20 21:15:27 2020 -0400
    Preserve mtimes from input tar files in TarFileWriter output tars (#155)
    * Preserve mtimes from input tar files in TarFileWriter output tars
    * Provide option for overriding input tar mtimes in TarFileWriter
    * Correctly set test fixture paths in archive test
-   commit 787f41777355ff2c0669e1a5a8771380d8752fa3

-   Author: Matt Mikitka <2027417+mmikitka@users.noreply.github.com>
    Date:   Fri Apr 17 12:31:15 2020 -0400
    Changed the buildroot from BUILD to BUILDROOT (#108)
    * Changed the buildroot from BUILD to BUILDROOT
    * Install files in the RPM spec before the %files section
-   commit ce53425bc5449268ade670346bc39d8c52b1f822

-   Author: Andrew Psaltis <apsaltis@vmware.com>
    Date:   Wed Apr 15 18:22:22 2020 -0400
    Add prototype pkgfilegroup-based RPM builder (#129)
    This change provides a prototype `pkgfilegroup`-based RPM builder in the form of
    the `gen_rpm` rule.  See #128 for more details on `pkgfilegroup`.
    The RPM generator was derived from `make_rpm.py` in `pkg/` and supports a number
    of features over and above what's available in `pkg_rpm`.  As written, it, given
    a template like the one provided, you can construct many full-fledged RPM
    packages entirely within Bazel.  In most cases, the templates will only need to
    be customized with advanced logic and other macros that are not settable via
    bazel itself; `gen_rpm` will write much of the preamble, `%description` text,
    `%install` scriptlets, and `%files` based on rule-provided inputs.
    Documentation outside of the source files is not yet available.  This was
    empirically tested on RPM packages internal to VMware with positive results;
    actual tests of the rules are not yet ready.
    This, naturally, is incomplete, and is missing capabilities such as:
    - Configurable compression
    - Configurable Provides/Requires
    - SRPM emission
    - Reproducibility
    - Configurable stripping
    - Configurable construction of "debug" packages
    Co-authored-by: mzeren-vmw <mzeren@vmware.com>
    Co-authored-by: klash <klash@vmware.com>
    Co-authored-by: mzeren-vmw <mzeren@vmware.com>
    Co-authored-by: klash <klash@vmware.com>
-   commit 79eafadca7b4fdb675b1cfa40b2ac20f23139271

-   Author: Matthias Frei <matthias.frei@inf.ethz.ch>
    Date:   Tue Apr 7 03:27:05 2020 +0200
    make_deb: fix length computation for extrafiles (#144)
    * make_deb: fix length computation for extrafiles
    Analogous to the handling of the main control file.
    * Add test for genrule-preinst with non-ascii chars
    * Fix tests

# Release 0.2.5

**New Features**

commit 48001d12e7037b04dc5b28fadfb1e10a8447e2fc
    Author: aiuto <aiuto@google.com>
    Date:   Thu Mar 12 15:14:32 2020 -0400

    Depend on rules_python (#140)

    * load rules python

    * add workspace deps

    * add missing loads

commit 2b375a08bfe36d2c35885a6f4e5b12d7898c9426
    Author: Ryan Beasley <39353016+beasleyr-vmw@users.noreply.github.com>
    Date:   Wed Mar 11 14:49:21 2020 -0400

    Update test data in response to #121 (#137)

    PR #121 changed pkg_deb's behavior but didn't update test data to match.

    Reported in PR #132.

    Testing Done:
    - `bazelisk test ...`

commit e5919f43791b2d4c5ab9e68786087cf889b9987e
    Author: Andrew Psaltis <ampsaltis@gmail.com>
    Date:   Fri Feb 28 01:22:37 2020 -0500

    Add pkgfilegroup for package-independent destination mappings (#128)

    * Add pkgfilegroup for package-independent destination mappings

    This adds an experimental rule, `pkgfilegroup`, along with associated Providers,
    that gives rule developers and users a consistent mechanism for using the output
    of bazel targets in packaging rules.

    Inspired by #36.

    Other capabilities that are provided by this that were not mentioned in #36 are:

    - Creation of empty directories (`pkg_mkdirs`)
    - Exclusion of files from a `pkgfilegroup` (`excludes`)
    - Renames of files in a `pkgfilegroup` (`renames`)

    * Add analysis tests for pkgfilegroup and friends

    This provides some analysis tests for various features of `pkgfilegroup` and
    `pkg_mkdirs`.  See #128.

    You can run them by invoking `bazel test experimental/...` from the `pkg`
    directory

    This implementation of pkgfilegroup was inspired by #36.

commit 7a991dea418ab17c7e86f0a7b5e7d4a87ef4304b
    Author: Ryan Beasley <39353016+beasleyr-vmw@users.noreply.github.com>
    Date:   Fri Feb 28 01:02:24 2020 -0500

    Improve handling of sources from external repositories (#132)

    Avoid use of [`File.short_path`][1] when mapping filenames, because when
    creating archives from files imported from external repositories we'll create
    archive members with leading `./../` prefixes.  Instead, we'll stick to stripping
    to leading `File.root.path` (if present) from `File.path`, resulting in archive
    members like `external/repo/package/file.txt`.

    [1]: https://docs.bazel.build/versions/master/skylark/lib/File.html#short_path

    Resolves #131.

commit 532f2857e712c5fcb71c662d680108685b242251
    Author: zoidbergwill <zoidbergwill@gmail.com>
    Date:   Fri Feb 28 06:56:05 2020 +0100

    Update pkg.bzl (#125)

commit 5877fa85b8598b5bb2186d3addca2408b1e61c5e
    Author: Matt Mikitka <2027417+mmikitka@users.noreply.github.com>
    Date:   Fri Feb 28 05:49:40 2020 +0000

    Rpm source date epoch (#113)

    * Added --source_date_epoch
    * Support source_date_epoch_file since stamp variables are not accessible
    * Fixed _make_rpm label
    * Revert default make_rpm label
    * Default source_date_epoch to None and remove os.environ

commit acc1ca9095e60bb9acd9858bc1812bfd805136df
    Author: Trevor Hickey <TrevorJamesHickey@gmail.com>
    Date:   Mon Feb 24 09:53:55 2020 -0500

    update WORKSPACE example (#124)

commit 2f5c9815a7bde4f18acfde268bd63fedd107d87c
    Author: andreas-0815-qwertz <57450822+andreas-0815-qwertz@users.noreply.github.com>
    Date:   Wed Dec 4 22:32:01 2019 +0100

    Add "./" prefix to files in control.tar (#121)

    This improves compatibility to Debian packages created using dpkg.

    https://github.com/bazelbuild/rules_pkg/issues/116

commit 2f09779667f0d6644c2ca5914d6113a82666ec63
    Author: Benjamin Peterson <benjamin@python.org>
    Date:   Fri Nov 15 10:09:45 2019 -0800

    pkg_deb: Support Breaks and Replaces. (#117)

    https://www.debian.org/doc/debian-policy/ch-relationships.html#overwriting-files-and-replacing-packages-replaces

commit 9192d3b3a0f6ccfdecdc66f08f0b2664fa0afc0f
   Author: Tony Aiuto <aiuto@google.com>
   Date:   Fri Oct 4 16:33:47 2019 -0400

    Fix repo names with '-' in them.

    We can not use the form "@repo-name" in Bazel, so the common solution is
    to transform that to "@repo_name". We auto-correct the repo names to the
    required form when printing the WORKSPACE stanza.
