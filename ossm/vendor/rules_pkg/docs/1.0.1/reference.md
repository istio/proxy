# rules_pkg - 1.0.0

<div class="toc">
  <h2>Common Attributes</h2>
  <ul>
    <li><a href="#common">Package attributes</a></li>
    <li><a href="#mapping-attrs">File attributes</a></li>
  </ul>

  <h2>Packaging Rules</h2>
  <ul>
    <li><a href="#pkg_deb">//pkg:deb.bzl%pkg_deb</a></li>
    <li><a href="#pkg_rpm">//pkg:rpm.bzl%pkg_rpm</a></li>
    <li><a href="#pkg_tar">//pkg:tar.bzl%pkg_tar</a></li>
    <li><a href="#pkg_zip">//pkg:zip.bzl%pkg_zip</a></li>
  </ul>

  <h2>File Tree Creation Rules</h2>
  <ul>
    <li><a href="#filter_directory">//pkg:mappings.bzl%filter_directory</a></li>
    <li><a href="#pkg_filegroup">//pkg:mappings.bzl%pkg_filegroup</a></li>
    <li><a href="#pkg_files">//pkg:mappings.bzl%pkg_files</a></li>
    <li><a href="#pkg_mkdirs">//pkg:mappings.bzl%pkg_mkdirs</a></li>
    <li><a href="#pkg_mklink">//pkg:mappings.bzl%pkg_mklink</a></li>
    <li><a href="#pkg_attributes">//pkg:mappings.bzl%pkg_attributes</a></li>
    <li><a href="#strip_prefix.files_only">//pkg:mappings.bzl%strip_prefix</a></li>
  </ul>
</div>

<a name="common"></a>

### Common Attributes

These attributes are used in several rules within this module.

**ATTRIBUTES**

| Name              | Description                                                                                                                                                                     | Type                                                               | Mandatory       | Default                                   |
| :-------------    | :-------------                                                                                                                                                                  | :-------------:                                                    | :-------------: | :-------------                            |
| <a name="out">out</a>               | Name of the output file. This file will always be created and used to access the package content. If `package_file_name` is also specified, `out` will be a symlink.            | String                                                             | required        |                                           |
| <a name="package_file_name">package_file_name</a> | The name of the file which will contain the package. The name may contain variables in the forms `{var}` and $(var)`. The values for substitution are specified through `package_variables` or taken from [ctx.var](https://bazel.build/rules/lib/ctx#var). | String | optional | package type specific |
| <a name="package_variables">package_variables</a> | A target that provides `PackageVariablesInfo` to substitute into `package_file_name`. `pkg_zip` and `pkg_tar` also support this in `package_dir`                                | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional        | None                                      |
| attributes        | Attributes to set on entities created within packages.  Not to be confused with bazel rule attributes.  See 'Mapping "Attributes"' below                                        | Undefined.                                                         | optional        | Varies.  Consult individual rule documentation for details. |

See
[examples/naming_package_files](https://github.com/bazelbuild/rules_pkg/tree/main/examples/naming_package_files)
for examples of how `out`, `package_file_name`, and `package_variables`
interact.

<div class="since"><i>Since 0.8.0</i></div>: File name substitution now supports the $(var) syntax.
<div class="since"><i>Since 0.8.0</i></div>: File name substitution now supports direct use of [ctx.var](https://bazel.build/rules/lib/ctx#var).


<a name="mapping-attrs"></a>
### Mapping "Attributes"

The "attributes" attribute specifies properties of package contents as used in
rules such as `pkg_files`, and `pkg_mkdirs`.  These allow fine-grained control
of the contents of your package.  For example:

```python
attributes = pkg_attributes(
    mode = "0644",
    user = "root",
    group = "wheel",
    my_custom_attribute = "some custom value",
)
```

`mode`, `user`, and `group` correspond to common UNIX-style filesystem
permissions.  Attributes should always be specified using the `pkg_attributes`
helper macro.

Each mapping rule has some default mapping attributes.  At this time, the only
default is "mode", which will be set if it is not otherwise overridden by the user.

If `user` and `group` are not specified, then defaults for them will be chosen
by the underlying package builder.  Any specific behavior from package builders
should not be relied upon.

Any other attributes should be specified as additional arguments to
`pkg_attributes`.

<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rule for creating Debian packages.

<a id="pkg_deb"></a>

## pkg_deb

<pre>
pkg_deb(<a href="#pkg_deb-name">name</a>, <a href="#pkg_deb-data">data</a>, <a href="#pkg_deb-out">out</a>, <a href="#pkg_deb-architecture">architecture</a>, <a href="#pkg_deb-architecture_file">architecture_file</a>, <a href="#pkg_deb-breaks">breaks</a>, <a href="#pkg_deb-built_using">built_using</a>,
             <a href="#pkg_deb-built_using_file">built_using_file</a>, <a href="#pkg_deb-changelog">changelog</a>, <a href="#pkg_deb-conffiles">conffiles</a>, <a href="#pkg_deb-conffiles_file">conffiles_file</a>, <a href="#pkg_deb-config">config</a>, <a href="#pkg_deb-conflicts">conflicts</a>, <a href="#pkg_deb-depends">depends</a>,
             <a href="#pkg_deb-depends_file">depends_file</a>, <a href="#pkg_deb-description">description</a>, <a href="#pkg_deb-description_file">description_file</a>, <a href="#pkg_deb-distribution">distribution</a>, <a href="#pkg_deb-enhances">enhances</a>, <a href="#pkg_deb-homepage">homepage</a>, <a href="#pkg_deb-license">license</a>,
             <a href="#pkg_deb-maintainer">maintainer</a>, <a href="#pkg_deb-package">package</a>, <a href="#pkg_deb-package_file_name">package_file_name</a>, <a href="#pkg_deb-package_variables">package_variables</a>, <a href="#pkg_deb-postinst">postinst</a>, <a href="#pkg_deb-postrm">postrm</a>, <a href="#pkg_deb-predepends">predepends</a>,
             <a href="#pkg_deb-preinst">preinst</a>, <a href="#pkg_deb-prerm">prerm</a>, <a href="#pkg_deb-priority">priority</a>, <a href="#pkg_deb-provides">provides</a>, <a href="#pkg_deb-recommends">recommends</a>, <a href="#pkg_deb-replaces">replaces</a>, <a href="#pkg_deb-section">section</a>, <a href="#pkg_deb-suggests">suggests</a>, <a href="#pkg_deb-templates">templates</a>,
             <a href="#pkg_deb-triggers">triggers</a>, <a href="#pkg_deb-urgency">urgency</a>, <a href="#pkg_deb-version">version</a>, <a href="#pkg_deb-version_file">version_file</a>)
</pre>

Create a Debian package.

This rule produces 2 artifacts: a .deb and a .changes file. The DefaultInfo will
include both. If you need downstream rule to specifically depend on only the .deb or
.changes file then you can use `filegroup` to select distinct output groups.

**OutputGroupInfo**
- `out` the Debian package or a symlink to the actual package.
- `deb` the package with any precise file name created with `package_file_name`.
- `changes` the .changes file.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_deb-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_deb-data"></a>data |  A tar file that contains the data for the debian package.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="pkg_deb-out"></a>out |  See [Common Attributes](#out)   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="pkg_deb-architecture"></a>architecture |  Package architecture. Must not be used with architecture_file.   | String | optional |  `"all"`  |
| <a id="pkg_deb-architecture_file"></a>architecture_file |  File that contains the package architecture. Must not be used with architecture.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-breaks"></a>breaks |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-built_using"></a>built_using |  The tool that were used to build this package provided either inline (with built_using) or from a file (with built_using_file).   | String | optional |  `""`  |
| <a id="pkg_deb-built_using_file"></a>built_using_file |  The tool that were used to build this package provided either inline (with built_using) or from a file (with built_using_file).   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-changelog"></a>changelog |  The package changelog. See https://www.debian.org/doc/debian-policy/ch-source.html#s-dpkgchangelog.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-conffiles"></a>conffiles |  The list of conffiles or a file containing one conffile per line. Each item is an absolute path on the target system where the deb is installed. See https://www.debian.org/doc/debian-policy/ch-files.html#s-config-files.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-conffiles_file"></a>conffiles_file |  The list of conffiles or a file containing one conffile per line. Each item is an absolute path on the target system where the deb is installed. See https://www.debian.org/doc/debian-policy/ch-files.html#s-config-files.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-config"></a>config |  config file used for debconf integration. See https://www.debian.org/doc/debian-policy/ch-binary.html#prompting-in-maintainer-scripts.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-conflicts"></a>conflicts |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-depends"></a>depends |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-depends_file"></a>depends_file |  File that contains a list of package dependencies. Must not be used with `depends`. See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-description"></a>description |  The package description. Must not be used with `description_file`.   | String | optional |  `""`  |
| <a id="pkg_deb-description_file"></a>description_file |  The package description. Must not be used with `description`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-distribution"></a>distribution |  "distribution: See http://www.debian.org/doc/debian-policy.   | String | optional |  `"unstable"`  |
| <a id="pkg_deb-enhances"></a>enhances |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-homepage"></a>homepage |  The homepage of the project.   | String | optional |  `""`  |
| <a id="pkg_deb-license"></a>license |  The license of the project.   | String | optional |  `""`  |
| <a id="pkg_deb-maintainer"></a>maintainer |  The maintainer of the package.   | String | required |  |
| <a id="pkg_deb-package"></a>package |  The name of the package   | String | required |  |
| <a id="pkg_deb-package_file_name"></a>package_file_name |  See [Common Attributes](#package_file_name). Default: "{package}-{version}-{architecture}.deb   | String | optional |  `""`  |
| <a id="pkg_deb-package_variables"></a>package_variables |  See [Common Attributes](#package_variables)   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-postinst"></a>postinst |  The post-install script for the package. See http://www.debian.org/doc/debian-policy/ch-maintainerscripts.html.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-postrm"></a>postrm |  The post-remove script for the package. See http://www.debian.org/doc/debian-policy/ch-maintainerscripts.html.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-predepends"></a>predepends |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-preinst"></a>preinst |  "The pre-install script for the package. See http://www.debian.org/doc/debian-policy/ch-maintainerscripts.html.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-prerm"></a>prerm |  The pre-remove script for the package. See http://www.debian.org/doc/debian-policy/ch-maintainerscripts.html.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-priority"></a>priority |  The priority of the package. See http://www.debian.org/doc/debian-policy/ch-archive.html#s-priorities.   | String | optional |  `""`  |
| <a id="pkg_deb-provides"></a>provides |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-recommends"></a>recommends |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-replaces"></a>replaces |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-section"></a>section |  The section of the package. See http://www.debian.org/doc/debian-policy/ch-archive.html#s-subsections.   | String | optional |  `""`  |
| <a id="pkg_deb-suggests"></a>suggests |  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps.   | List of strings | optional |  `[]`  |
| <a id="pkg_deb-templates"></a>templates |  templates file used for debconf integration. See https://www.debian.org/doc/debian-policy/ch-binary.html#prompting-in-maintainer-scripts.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-triggers"></a>triggers |  triggers file for configuring installation events exchanged by packages. See https://wiki.debian.org/DpkgTriggers.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_deb-urgency"></a>urgency |  "urgency: See http://www.debian.org/doc/debian-policy.   | String | optional |  `"medium"`  |
| <a id="pkg_deb-version"></a>version |  Package version. Must not be used with `version_file`.   | String | optional |  `""`  |
| <a id="pkg_deb-version_file"></a>version_file |  File that contains the package version. Must not be used with `version`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |



<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Provides rules for creating RPM packages via pkg_filegroup and friends.

pkg_rpm() depends on the existence of an rpmbuild toolchain. Many users will
find to convenient to use the one provided with their system. To enable that
toolchain add the following stanza to WORKSPACE:

```
# Find rpmbuild if it exists.
load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")
find_system_rpmbuild(name="rules_pkg_rpmbuild")
```

<a id="pkg_sub_rpm"></a>

## pkg_sub_rpm

<pre>
pkg_sub_rpm(<a href="#pkg_sub_rpm-name">name</a>, <a href="#pkg_sub_rpm-srcs">srcs</a>, <a href="#pkg_sub_rpm-architecture">architecture</a>, <a href="#pkg_sub_rpm-conflicts">conflicts</a>, <a href="#pkg_sub_rpm-description">description</a>, <a href="#pkg_sub_rpm-epoch">epoch</a>, <a href="#pkg_sub_rpm-group">group</a>, <a href="#pkg_sub_rpm-obsoletes">obsoletes</a>, <a href="#pkg_sub_rpm-package_name">package_name</a>,
            <a href="#pkg_sub_rpm-post_scriptlet">post_scriptlet</a>, <a href="#pkg_sub_rpm-provides">provides</a>, <a href="#pkg_sub_rpm-requires">requires</a>, <a href="#pkg_sub_rpm-summary">summary</a>, <a href="#pkg_sub_rpm-version">version</a>)
</pre>

Define a sub RPM to be built as part of a parent RPM

This rule uses the outputs of the rules in `mappings.bzl` to define an sub
RPM that will be built as part of a larger RPM defined by a `pkg_rpm` instance.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_sub_rpm-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_sub_rpm-srcs"></a>srcs |  Mapping groups to include in this RPM   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="pkg_sub_rpm-architecture"></a>architecture |  Sub RPM architecture   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-conflicts"></a>conflicts |  List of RPM capability expressions that conflict with this package   | List of strings | optional |  `[]`  |
| <a id="pkg_sub_rpm-description"></a>description |  Multi-line description of this subrpm   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-epoch"></a>epoch |  RPM `Epoch` tag for this subrpm   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-group"></a>group |  Optional; RPM "Group" tag.<br><br>NOTE: some distributions (as of writing, Fedora > 17 and CentOS/RHEL > 5) have deprecated this tag.  Other distributions may require it, but it is harmless in any case.   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-obsoletes"></a>obsoletes |  List of RPM capability expressions that this package obsoletes   | List of strings | optional |  `[]`  |
| <a id="pkg_sub_rpm-package_name"></a>package_name |  name of the subrpm   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-post_scriptlet"></a>post_scriptlet |  RPM `%post` scriplet for this subrpm   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-provides"></a>provides |  List of RPM capability expressions that this package provides   | List of strings | optional |  `[]`  |
| <a id="pkg_sub_rpm-requires"></a>requires |  List of RPM capability expressions that this package requires   | List of strings | optional |  `[]`  |
| <a id="pkg_sub_rpm-summary"></a>summary |  Sub RPM `Summary` tag   | String | optional |  `""`  |
| <a id="pkg_sub_rpm-version"></a>version |  RPM `Version` tag for this subrpm   | String | optional |  `""`  |



<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Provides rules for creating RPM packages via pkg_filegroup and friends.

pkg_rpm() depends on the existence of an rpmbuild toolchain. Many users will
find to convenient to use the one provided with their system. To enable that
toolchain add the following stanza to WORKSPACE:

```
# Find rpmbuild if it exists.
load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")
find_system_rpmbuild(name="rules_pkg_rpmbuild")
```

<a id="pkg_rpm"></a>

## pkg_rpm

<pre>
pkg_rpm(<a href="#pkg_rpm-name">name</a>, <a href="#pkg_rpm-srcs">srcs</a>, <a href="#pkg_rpm-architecture">architecture</a>, <a href="#pkg_rpm-binary_payload_compression">binary_payload_compression</a>, <a href="#pkg_rpm-changelog">changelog</a>, <a href="#pkg_rpm-conflicts">conflicts</a>, <a href="#pkg_rpm-debug">debug</a>,
        <a href="#pkg_rpm-debuginfo">debuginfo</a>, <a href="#pkg_rpm-defines">defines</a>, <a href="#pkg_rpm-description">description</a>, <a href="#pkg_rpm-description_file">description_file</a>, <a href="#pkg_rpm-epoch">epoch</a>, <a href="#pkg_rpm-group">group</a>, <a href="#pkg_rpm-license">license</a>, <a href="#pkg_rpm-obsoletes">obsoletes</a>,
        <a href="#pkg_rpm-package_file_name">package_file_name</a>, <a href="#pkg_rpm-package_name">package_name</a>, <a href="#pkg_rpm-package_variables">package_variables</a>, <a href="#pkg_rpm-post_scriptlet">post_scriptlet</a>, <a href="#pkg_rpm-post_scriptlet_file">post_scriptlet_file</a>,
        <a href="#pkg_rpm-posttrans_scriptlet">posttrans_scriptlet</a>, <a href="#pkg_rpm-posttrans_scriptlet_file">posttrans_scriptlet_file</a>, <a href="#pkg_rpm-postun_scriptlet">postun_scriptlet</a>, <a href="#pkg_rpm-postun_scriptlet_file">postun_scriptlet_file</a>,
        <a href="#pkg_rpm-pre_scriptlet">pre_scriptlet</a>, <a href="#pkg_rpm-pre_scriptlet_file">pre_scriptlet_file</a>, <a href="#pkg_rpm-preun_scriptlet">preun_scriptlet</a>, <a href="#pkg_rpm-preun_scriptlet_file">preun_scriptlet_file</a>, <a href="#pkg_rpm-provides">provides</a>, <a href="#pkg_rpm-release">release</a>,
        <a href="#pkg_rpm-release_file">release_file</a>, <a href="#pkg_rpm-requires">requires</a>, <a href="#pkg_rpm-requires_contextual">requires_contextual</a>, <a href="#pkg_rpm-rpmbuild_path">rpmbuild_path</a>, <a href="#pkg_rpm-source_date_epoch">source_date_epoch</a>,
        <a href="#pkg_rpm-source_date_epoch_file">source_date_epoch_file</a>, <a href="#pkg_rpm-spec_template">spec_template</a>, <a href="#pkg_rpm-subrpms">subrpms</a>, <a href="#pkg_rpm-summary">summary</a>, <a href="#pkg_rpm-url">url</a>, <a href="#pkg_rpm-version">version</a>, <a href="#pkg_rpm-version_file">version_file</a>)
</pre>

Creates an RPM format package via `pkg_filegroup` and friends.

The uses the outputs of the rules in `mappings.bzl` to construct arbitrary
RPM packages.  Attributes of this rule provide preamble information and
scriptlets, which are then used to compose a valid RPM spec file.

This rule will fail at analysis time if:

- Any `srcs` input creates the same destination, regardless of other
  attributes.

This rule only functions on UNIXy platforms. The following tools must be
available on your system for this to function properly:

- `rpmbuild` (as specified in `rpmbuild_path`, or available in `$PATH`)

- GNU coreutils.  BSD coreutils may work, but are not tested.

To set RPM file attributes (like `%config` and friends), set the
`rpm_filetag` in corresponding packaging rule (`pkg_files`, etc).  The value
is prepended with "%" and added to the `%files` list, for example:

```
attrs = {"rpm_filetag": ("config(missingok, noreplace)",)},
```

Is the equivalent to `%config(missingok, noreplace)` in the `%files` list.

This rule produces 2 artifacts: an .rpm and a .changes file. The DefaultInfo will
include both. If you need downstream rule to specifically depend on only the .rpm or
.changes file then you can use `filegroup` to select distinct output groups.

**OutputGroupInfo**
- `out` the RPM or a symlink to the actual package.
- `rpm` the package with any precise file name created with `package_file_name`.
- `changes` the .changes file.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_rpm-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_rpm-srcs"></a>srcs |  Mapping groups to include in this RPM.<br><br>These are typically brought into life as `pkg_filegroup`s.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="pkg_rpm-architecture"></a>architecture |  Package architecture.<br><br>This currently sets the `BuildArch` tag, which influences the output architecture of the package.<br><br>Typically, `BuildArch` only needs to be set when the package is known to be cross-platform (e.g. written in an interpreted language), or, less common, when it is known that the application is only valid for specific architectures.<br><br>When no attribute is provided, this will default to your host's architecture.  This is usually what you want.   | String | optional |  `""`  |
| <a id="pkg_rpm-binary_payload_compression"></a>binary_payload_compression |  Compression mode used for this RPM<br><br>Must be a form that `rpmbuild(8)` knows how to process, which will depend on the version of `rpmbuild` in use.  The value corresponds to the `%_binary_payload` macro and is set on the `rpmbuild(8)` command line if provided.<br><br>Some examples of valid values (which may not be supported on your system) can be found [here](https://git.io/JU9Wg).  On CentOS systems (also likely Red Hat and Fedora), you can find some supported values by looking for `%_binary_payload` in `/usr/lib/rpm/macros`.  Other systems have similar files and configurations.<br><br>If not provided, the compression mode will be computed by `rpmbuild` itself.  Defaults may vary per distribution or build of `rpm`; consult the relevant documentation for more details.<br><br>WARNING: Bazel is currently not aware of action threading requirements for non-test actions.  Using threaded compression may result in overcommitting your system.   | String | optional |  `""`  |
| <a id="pkg_rpm-changelog"></a>changelog |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-conflicts"></a>conflicts |  List of capabilities that conflict with this package when it is installed.<br><br>Corresponds to the "Conflicts" preamble tag.<br><br>See also: https://rpm-software-management.github.io/rpm/manual/dependencies.html   | List of strings | optional |  `[]`  |
| <a id="pkg_rpm-debug"></a>debug |  Debug the RPM helper script and RPM generation   | Boolean | optional |  `False`  |
| <a id="pkg_rpm-debuginfo"></a>debuginfo |  Enable generation of debuginfo RPMs<br><br>For supported platforms this will enable the generation of debuginfo RPMs adjacent to the regular RPMs.  Currently this is supported by Fedora 40, CentOS7 and CentOS Stream 9.   | Boolean | optional |  `False`  |
| <a id="pkg_rpm-defines"></a>defines |  Additional definitions to pass to rpmbuild   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="pkg_rpm-description"></a>description |  Multi-line description of this package, corresponds to RPM %description.<br><br>Exactly one of `description` or `description_file` must be provided.   | String | optional |  `""`  |
| <a id="pkg_rpm-description_file"></a>description_file |  File containing a multi-line description of this package, corresponds to RPM %description.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-epoch"></a>epoch |  Optional; RPM "Epoch" tag.   | String | optional |  `""`  |
| <a id="pkg_rpm-group"></a>group |  Optional; RPM "Group" tag.<br><br>NOTE: some distributions (as of writing, Fedora > 17 and CentOS/RHEL > 5) have deprecated this tag.  Other distributions may require it, but it is harmless in any case.   | String | optional |  `""`  |
| <a id="pkg_rpm-license"></a>license |  RPM "License" tag.<br><br>The software license for the code distributed in this package.<br><br>The underlying RPM builder requires you to put something here; if your package is not going to be distributed, feel free to set this to something like "Internal".   | String | required |  |
| <a id="pkg_rpm-obsoletes"></a>obsoletes |  List of rpm capability expressions that this package obsoletes.<br><br>Corresponds to the "Obsoletes" preamble tag.<br><br>See also: https://rpm-software-management.github.io/rpm/manual/dependencies.html   | List of strings | optional |  `[]`  |
| <a id="pkg_rpm-package_file_name"></a>package_file_name |  See 'Common Attributes' in the rules_pkg reference.<br><br>If this is not provided, the package file given a NVRA-style (name-version-release.arch) output, which is preferred by most RPM repositories.   | String | optional |  `""`  |
| <a id="pkg_rpm-package_name"></a>package_name |  Optional; RPM name override.<br><br>If not provided, the `name` attribute of this rule will be used instead.<br><br>This influences values like the spec file name.   | String | optional |  `""`  |
| <a id="pkg_rpm-package_variables"></a>package_variables |  See 'Common Attributes' in the rules_pkg reference   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-post_scriptlet"></a>post_scriptlet |  RPM `%post` scriptlet.  Currently only allowed to be a shell script.<br><br>`post_scriptlet` and `post_scriptlet_file` are mutually exclusive.   | String | optional |  `""`  |
| <a id="pkg_rpm-post_scriptlet_file"></a>post_scriptlet_file |  File containing the RPM `%post` scriptlet   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-posttrans_scriptlet"></a>posttrans_scriptlet |  RPM `%posttrans` scriptlet.  Currently only allowed to be a shell script.<br><br>`posttrans_scriptlet` and `posttrans_scriptlet_file` are mutually exclusive.   | String | optional |  `""`  |
| <a id="pkg_rpm-posttrans_scriptlet_file"></a>posttrans_scriptlet_file |  File containing the RPM `%posttrans` scriptlet   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-postun_scriptlet"></a>postun_scriptlet |  RPM `%postun` scriptlet.  Currently only allowed to be a shell script.<br><br>`postun_scriptlet` and `postun_scriptlet_file` are mutually exclusive.   | String | optional |  `""`  |
| <a id="pkg_rpm-postun_scriptlet_file"></a>postun_scriptlet_file |  File containing the RPM `%postun` scriptlet   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-pre_scriptlet"></a>pre_scriptlet |  RPM `%pre` scriptlet.  Currently only allowed to be a shell script.<br><br>`pre_scriptlet` and `pre_scriptlet_file` are mutually exclusive.   | String | optional |  `""`  |
| <a id="pkg_rpm-pre_scriptlet_file"></a>pre_scriptlet_file |  File containing the RPM `%pre` scriptlet   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-preun_scriptlet"></a>preun_scriptlet |  RPM `%preun` scriptlet.  Currently only allowed to be a shell script.<br><br>`preun_scriptlet` and `preun_scriptlet_file` are mutually exclusive.   | String | optional |  `""`  |
| <a id="pkg_rpm-preun_scriptlet_file"></a>preun_scriptlet_file |  File containing the RPM `%preun` scriptlet   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-provides"></a>provides |  List of rpm capabilities that this package provides.<br><br>Corresponds to the "Provides" preamble tag.<br><br>See also: https://rpm-software-management.github.io/rpm/manual/dependencies.html   | List of strings | optional |  `[]`  |
| <a id="pkg_rpm-release"></a>release |  RPM "Release" tag<br><br>Exactly one of `release` or `release_file` must be provided.   | String | optional |  `""`  |
| <a id="pkg_rpm-release_file"></a>release_file |  File containing RPM "Release" tag.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-requires"></a>requires |  List of rpm capability expressions that this package requires.<br><br>Corresponds to the "Requires" preamble tag.<br><br>See also: https://rpm-software-management.github.io/rpm/manual/dependencies.html   | List of strings | optional |  `[]`  |
| <a id="pkg_rpm-requires_contextual"></a>requires_contextual |  Contextualized requirement specifications<br><br>This is a map of various properties (often scriptlet types) to capability name specifications, e.g.:<br><br><pre><code class="language-python">{"pre": ["GConf2"],"post": ["GConf2"], "postun": ["GConf2"]}</code></pre><br><br>Which causes the below to be added to the spec file's preamble:<br><br><pre><code>Requires(pre): GConf2&#10;Requires(post): GConf2&#10;Requires(postun): GConf2</code></pre><br><br>This is most useful for ensuring that required tools exist when scriptlets are run, although there may be other valid use cases. Valid keys for this attribute may include, but are not limited to:<br><br>- `pre` - `post` - `preun` - `postun` - `pretrans` - `posttrans`<br><br>For capabilities that are always required by packages at runtime, use the `requires` attribute instead.<br><br>See also: https://rpm-software-management.github.io/rpm/manual/more_dependencies.html<br><br>NOTE: `pkg_rpm` does not check if the keys of this dictionary are acceptable to `rpm(8)`.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> List of strings</a> | optional |  `{}`  |
| <a id="pkg_rpm-rpmbuild_path"></a>rpmbuild_path |  Path to a `rpmbuild` binary.  Deprecated in favor of the rpmbuild toolchain   | String | optional |  `""`  |
| <a id="pkg_rpm-source_date_epoch"></a>source_date_epoch |  Value to export as SOURCE_DATE_EPOCH to facilitate reproducible builds<br><br>Implicitly sets the `%clamp_mtime_to_source_date_epoch` in the subordinate call to `rpmbuild` to facilitate more consistent in-RPM file timestamps.<br><br>Negative values (like the default) disable this feature.   | Integer | optional |  `-1`  |
| <a id="pkg_rpm-source_date_epoch_file"></a>source_date_epoch_file |  File containing the SOURCE_DATE_EPOCH value.<br><br>Implicitly sets the `%clamp_mtime_to_source_date_epoch` in the subordinate call to `rpmbuild` to facilitate more consistent in-RPM file timestamps.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-spec_template"></a>spec_template |  Spec file template.<br><br>Use this if you need to add additional logic to your spec files that is not available by default.<br><br>In most cases, you should not need to override this attribute.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `"@rules_pkg//pkg/rpm:template.spec.tpl"`  |
| <a id="pkg_rpm-subrpms"></a>subrpms |  Sub RPMs to build with this RPM<br><br>A list of `pkg_sub_rpm` instances that can be used to create sub RPMs as part of the overall package build.<br><br>NOTE: use of `subrpms` is incompatible with the legacy `spec_file` mode   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="pkg_rpm-summary"></a>summary |  RPM "Summary" tag.<br><br>One-line summary of this package.  Must not contain newlines.   | String | required |  |
| <a id="pkg_rpm-url"></a>url |  RPM "URL" tag; this project/vendor's home on the Internet.   | String | optional |  `""`  |
| <a id="pkg_rpm-version"></a>version |  RPM "Version" tag.<br><br>Exactly one of `version` or `version_file` must be provided.   | String | optional |  `""`  |
| <a id="pkg_rpm-version_file"></a>version_file |  File containing RPM "Version" tag.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |



<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rules for making .tar files.

<a id="pkg_tar"></a>

## pkg_tar

<pre>
pkg_tar(<a href="#pkg_tar-name">name</a>, <a href="#pkg_tar-deps">deps</a>, <a href="#pkg_tar-srcs">srcs</a>, <a href="#pkg_tar-out">out</a>, <a href="#pkg_tar-allow_duplicates_from_deps">allow_duplicates_from_deps</a>,
             <a href="#pkg_tar-allow_duplicates_with_different_content">allow_duplicates_with_different_content</a>, <a href="#pkg_tar-compressor">compressor</a>, <a href="#pkg_tar-compressor_args">compressor_args</a>, <a href="#pkg_tar-create_parents">create_parents</a>,
             <a href="#pkg_tar-empty_dirs">empty_dirs</a>, <a href="#pkg_tar-empty_files">empty_files</a>, <a href="#pkg_tar-extension">extension</a>, <a href="#pkg_tar-files">files</a>, <a href="#pkg_tar-include_runfiles">include_runfiles</a>, <a href="#pkg_tar-mode">mode</a>, <a href="#pkg_tar-modes">modes</a>, <a href="#pkg_tar-mtime">mtime</a>, <a href="#pkg_tar-owner">owner</a>,
             <a href="#pkg_tar-ownername">ownername</a>, <a href="#pkg_tar-ownernames">ownernames</a>, <a href="#pkg_tar-owners">owners</a>, <a href="#pkg_tar-package_dir">package_dir</a>, <a href="#pkg_tar-package_dir_file">package_dir_file</a>, <a href="#pkg_tar-package_file_name">package_file_name</a>,
             <a href="#pkg_tar-package_variables">package_variables</a>, <a href="#pkg_tar-portable_mtime">portable_mtime</a>, <a href="#pkg_tar-private_stamp_detect">private_stamp_detect</a>, <a href="#pkg_tar-remap_paths">remap_paths</a>, <a href="#pkg_tar-stamp">stamp</a>,
             <a href="#pkg_tar-strip_prefix">strip_prefix</a>, <a href="#pkg_tar-symlinks">symlinks</a>)
</pre>



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_tar-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_tar-deps"></a>deps |  tar files which will be unpacked and repacked into the archive.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="pkg_tar-srcs"></a>srcs |  Inputs which will become part of the tar archive.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="pkg_tar-out"></a>out |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="pkg_tar-allow_duplicates_from_deps"></a>allow_duplicates_from_deps |  -   | Boolean | optional |  `False`  |
| <a id="pkg_tar-allow_duplicates_with_different_content"></a>allow_duplicates_with_different_content |  If true, will allow you to reference multiple pkg_* which conflict (writing different content or metadata to the same destination). Such behaviour is always incorrect, but we provide a flag to support it in case old builds were accidentally doing it. Never explicitly set this to true for new code.   | Boolean | optional |  `True`  |
| <a id="pkg_tar-compressor"></a>compressor |  External tool which can compress the archive.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_tar-compressor_args"></a>compressor_args |  Arg list for `compressor`.   | String | optional |  `""`  |
| <a id="pkg_tar-create_parents"></a>create_parents |  -   | Boolean | optional |  `True`  |
| <a id="pkg_tar-empty_dirs"></a>empty_dirs |  -   | List of strings | optional |  `[]`  |
| <a id="pkg_tar-empty_files"></a>empty_files |  -   | List of strings | optional |  `[]`  |
| <a id="pkg_tar-extension"></a>extension |  -   | String | optional |  `"tar"`  |
| <a id="pkg_tar-files"></a>files |  Obsolete. Do not use.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="pkg_tar-include_runfiles"></a>include_runfiles |  Include runfiles for executables. These appear as they would in bazel-bin.For example: 'path/to/myprog.runfiles/path/to/my_data.txt'.   | Boolean | optional |  `False`  |
| <a id="pkg_tar-mode"></a>mode |  -   | String | optional |  `"0555"`  |
| <a id="pkg_tar-modes"></a>modes |  -   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="pkg_tar-mtime"></a>mtime |  -   | Integer | optional |  `-1`  |
| <a id="pkg_tar-owner"></a>owner |  Default numeric owner.group to apply to files when not set via pkg_attributes.   | String | optional |  `"0.0"`  |
| <a id="pkg_tar-ownername"></a>ownername |  -   | String | optional |  `"."`  |
| <a id="pkg_tar-ownernames"></a>ownernames |  -   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="pkg_tar-owners"></a>owners |  -   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="pkg_tar-package_dir"></a>package_dir |  Prefix to be prepend to all paths written.<br><br>This is applied as a final step, while writing to the archive. Any other attributes (e.g. symlinks) which specify a path, must do so relative to package_dir. The value may contain variables. See [package_file_name](#package_file_name) for examples.   | String | optional |  `""`  |
| <a id="pkg_tar-package_dir_file"></a>package_dir_file |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_tar-package_file_name"></a>package_file_name |  See [Common Attributes](#package_file_name)   | String | optional |  `""`  |
| <a id="pkg_tar-package_variables"></a>package_variables |  See [Common Attributes](#package_variables)   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_tar-portable_mtime"></a>portable_mtime |  -   | Boolean | optional |  `True`  |
| <a id="pkg_tar-private_stamp_detect"></a>private_stamp_detect |  -   | Boolean | optional |  `False`  |
| <a id="pkg_tar-remap_paths"></a>remap_paths |  -   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="pkg_tar-stamp"></a>stamp |  Enable file time stamping.  Possible values: <li>stamp = 1: Use the time of the build as the modification time of each file in the archive. <li>stamp = 0: Use an "epoch" time for the modification time of each file. This gives good build result caching. <li>stamp = -1: Control the chosen modification time using the --[no]stamp flag. <div class="since"><i>Since 0.5.0</i></div>   | Integer | optional |  `0`  |
| <a id="pkg_tar-strip_prefix"></a>strip_prefix |  (note: Use strip_prefix = "." to strip path to the package but preserve relative paths of sub directories beneath the package.)   | String | optional |  `""`  |
| <a id="pkg_tar-symlinks"></a>symlinks |  -   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |



<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Zip archive creation rule and associated logic.

<a id="pkg_zip"></a>

## pkg_zip

<pre>
pkg_zip(<a href="#pkg_zip-name">name</a>, <a href="#pkg_zip-srcs">srcs</a>, <a href="#pkg_zip-out">out</a>, <a href="#pkg_zip-allow_duplicates_with_different_content">allow_duplicates_with_different_content</a>, <a href="#pkg_zip-compression_level">compression_level</a>,
             <a href="#pkg_zip-compression_type">compression_type</a>, <a href="#pkg_zip-include_runfiles">include_runfiles</a>, <a href="#pkg_zip-mode">mode</a>, <a href="#pkg_zip-package_dir">package_dir</a>, <a href="#pkg_zip-package_file_name">package_file_name</a>,
             <a href="#pkg_zip-package_variables">package_variables</a>, <a href="#pkg_zip-private_stamp_detect">private_stamp_detect</a>, <a href="#pkg_zip-stamp">stamp</a>, <a href="#pkg_zip-strip_prefix">strip_prefix</a>, <a href="#pkg_zip-timestamp">timestamp</a>)
</pre>



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_zip-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_zip-srcs"></a>srcs |  List of files that should be included in the archive.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="pkg_zip-out"></a>out |  output file name. Default: name + ".zip".   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="pkg_zip-allow_duplicates_with_different_content"></a>allow_duplicates_with_different_content |  If true, will allow you to reference multiple pkg_* which conflict (writing different content or metadata to the same destination). Such behaviour is always incorrect, but we provide a flag to support it in case old builds were accidentally doing it. Never explicitly set this to true for new code.   | Boolean | optional |  `True`  |
| <a id="pkg_zip-compression_level"></a>compression_level |  The compression level to use, 1 is the fastest, 9 gives the smallest results. 0 skips compression, depending on the method used   | Integer | optional |  `6`  |
| <a id="pkg_zip-compression_type"></a>compression_type |  The compression to use. Note that lzma and bzip2 might not be supported by all readers. The list of compressions is the same as Python's ZipFile: https://docs.python.org/3/library/zipfile.html#zipfile.ZIP_STORED   | String | optional |  `"deflated"`  |
| <a id="pkg_zip-include_runfiles"></a>include_runfiles |  See standard attributes.   | Boolean | optional |  `False`  |
| <a id="pkg_zip-mode"></a>mode |  The default mode for all files in the archive.   | String | optional |  `"0555"`  |
| <a id="pkg_zip-package_dir"></a>package_dir |  Prefix to be prepend to all paths written. The name may contain variables, same as [package_file_name](#package_file_name)   | String | optional |  `"/"`  |
| <a id="pkg_zip-package_file_name"></a>package_file_name |  See [Common Attributes](#package_file_name)   | String | optional |  `""`  |
| <a id="pkg_zip-package_variables"></a>package_variables |  See [Common Attributes](#package_variables)   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_zip-private_stamp_detect"></a>private_stamp_detect |  -   | Boolean | optional |  `False`  |
| <a id="pkg_zip-stamp"></a>stamp |  Enable file time stamping.  Possible values: <li>stamp = 1: Use the time of the build as the modification time of each file in the archive. <li>stamp = 0: Use an "epoch" time for the modification time of each file. This gives good build result caching. <li>stamp = -1: Control the chosen modification time using the --[no]stamp flag.   | Integer | optional |  `0`  |
| <a id="pkg_zip-strip_prefix"></a>strip_prefix |  -   | String | optional |  `""`  |
| <a id="pkg_zip-timestamp"></a>timestamp |  Time stamp to place on all files in the archive, expressed as seconds since the Unix Epoch, as per RFC 3339.  The default is January 01, 1980, 00:00 UTC.<br><br>Due to limitations in the format of zip files, values before Jan 1, 1980 will be rounded up and the precision in the zip file is limited to a granularity of 2 seconds.   | Integer | optional |  `315532800`  |



<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Package creation helper mapping rules.

This module declares Provider interfaces and rules for specifying the contents
of packages in a package-type-agnostic way.  The main rules supported here are
the following:

- `pkg_files` describes destinations for rule outputs
- `pkg_mkdirs` describes directory structures
- `pkg_mklink` describes symbolic links
- `pkg_filegroup` creates groupings of above to add to packages

Rules that actually make use of the outputs of the above rules are not specified
here.

<a id="filter_directory"></a>

## filter_directory

<pre>
filter_directory(<a href="#filter_directory-name">name</a>, <a href="#filter_directory-src">src</a>, <a href="#filter_directory-excludes">excludes</a>, <a href="#filter_directory-outdir_name">outdir_name</a>, <a href="#filter_directory-prefix">prefix</a>, <a href="#filter_directory-renames">renames</a>, <a href="#filter_directory-strip_prefix">strip_prefix</a>)
</pre>

Transform directories (TreeArtifacts) using pkg_filegroup-like semantics.

Effective order of operations:

1) Files are `exclude`d
2) `renames` _or_ `strip_prefix` is applied.
3) `prefix` is applied

In particular, if a `rename` applies to an individual file, `strip_prefix`
will not be applied to that particular file.

Each non-`rename``d path will look like this:

```
$OUTPUT_DIR/$PREFIX/$FILE_WITHOUT_STRIP_PREFIX
```

Each `rename`d path will look like this:

```
$OUTPUT_DIR/$PREFIX/$FILE_RENAMED
```

If an operation cannot be applied (`strip_prefix`) to any component in the
directory, or if one is unused (`exclude`, `rename`), the underlying command
will fail.  See the individual attributes for details.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="filter_directory-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="filter_directory-src"></a>src |  Directory (TreeArtifact) to process.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="filter_directory-excludes"></a>excludes |  Files to exclude from the output directory.<br><br>Each element must refer to an individual file in `src`.<br><br>All exclusions must be used.   | List of strings | optional |  `[]`  |
| <a id="filter_directory-outdir_name"></a>outdir_name |  Name of output directory (otherwise defaults to the rule's name)   | String | optional |  `""`  |
| <a id="filter_directory-prefix"></a>prefix |  Prefix to add to all paths in the output directory.<br><br>This does not include the output directory name, which will be added regardless.   | String | optional |  `""`  |
| <a id="filter_directory-renames"></a>renames |  Files to rename in the output directory.<br><br>Keys are destinations, values are sources prior to any path modifications (e.g. via `prefix` or `strip_prefix`).  Files that are `exclude`d must not be renamed.<br><br>This currently only operates on individual files.  `strip_prefix` does not apply to them.<br><br>All renames must be used.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="filter_directory-strip_prefix"></a>strip_prefix |  Prefix to remove from all paths in the output directory.<br><br>Must apply to all paths in the directory, even those rename'd.   | String | optional |  `""`  |


<a id="pkg_filegroup"></a>

## pkg_filegroup

<pre>
pkg_filegroup(<a href="#pkg_filegroup-name">name</a>, <a href="#pkg_filegroup-srcs">srcs</a>, <a href="#pkg_filegroup-prefix">prefix</a>)
</pre>

Package contents grouping rule.

This rule represents a collection of packaging specifications (e.g. those
created by `pkg_files`, `pkg_mklink`, etc.) that have something in common,
such as a prefix or a human-readable category.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_filegroup-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_filegroup-srcs"></a>srcs |  A list of packaging specifications to be grouped together.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="pkg_filegroup-prefix"></a>prefix |  A prefix to prepend to provided paths, applied like so:<br><br>- For files and directories, this is simply prepended to the destination - For symbolic links, this is prepended to the "destination" part.   | String | optional |  `""`  |


<a id="pkg_files"></a>

## pkg_files

<pre>
pkg_files(<a href="#pkg_files-name">name</a>, <a href="#pkg_files-srcs">srcs</a>, <a href="#pkg_files-attributes">attributes</a>, <a href="#pkg_files-excludes">excludes</a>, <a href="#pkg_files-include_runfiles">include_runfiles</a>, <a href="#pkg_files-prefix">prefix</a>, <a href="#pkg_files-renames">renames</a>, <a href="#pkg_files-strip_prefix">strip_prefix</a>)
</pre>

General-purpose package target-to-destination mapping rule.

This rule provides a specification for the locations and attributes of
targets when they are packaged. No outputs are created other than Providers
that are intended to be consumed by other packaging rules, such as
`pkg_rpm`. `pkg_files` targets may be consumed by other `pkg_files` or
`pkg_filegroup` to build up complex layouts, or directly by top level
packaging rules such as `pkg_files`.

Consumers of `pkg_files`s will, where possible, create the necessary
directory structure for your files so you do not have to unless you have
special requirements.  Consult `pkg_mkdirs` for more details.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_files-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_files-srcs"></a>srcs |  Files/Labels to include in the outputs of these rules   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="pkg_files-attributes"></a>attributes |  Attributes to set on packaged files.<br><br>Always use `pkg_attributes()` to set this rule attribute.<br><br>If not otherwise overridden, the file's mode will be set to UNIX "0644", or the target platform's equivalent.<br><br>Consult the "Mapping Attributes" documentation in the rules_pkg reference for more details.   | String | optional |  `"{}"`  |
| <a id="pkg_files-excludes"></a>excludes |  List of files or labels to exclude from the inputs to this rule.<br><br>Mostly useful for removing files from generated outputs or preexisting `filegroup`s.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="pkg_files-include_runfiles"></a>include_runfiles |  Add runfiles for all srcs.<br><br>The runfiles are in the paths that Bazel uses. For example, for the target `//my_prog:foo`, we would see files under paths like `foo.runfiles/<repo name>/my_prog/<file>`   | Boolean | optional |  `False`  |
| <a id="pkg_files-prefix"></a>prefix |  Installation prefix.<br><br>This may be an arbitrary string, but it should be understandable by the packaging system you are using to have the desired outcome.  For example, RPM macros like `%{_libdir}` may work correctly in paths for RPM packages, not, say, Debian packages.<br><br>If any part of the directory structure of the computed destination of a file provided to `pkg_filegroup` or any similar rule does not already exist within a package, the package builder will create it for you with a reasonable set of default permissions (typically `0755 root.root`).<br><br>It is possible to establish directory structures with arbitrary permissions using `pkg_mkdirs`.   | String | optional |  `""`  |
| <a id="pkg_files-renames"></a>renames |  Destination override map.<br><br>This attribute allows the user to override destinations of files in `pkg_file`s relative to the `prefix` attribute.  Keys to the dict are source files/labels, values are destinations relative to the `prefix`, ignoring whatever value was provided for `strip_prefix`.<br><br>If the key refers to a TreeArtifact (directory output), you may specify the constant `REMOVE_BASE_DIRECTORY` as the value, which will result in all containing files and directories being installed relative to the otherwise specified install prefix (via the `prefix` and `strip_prefix` attributes), not the directory name.<br><br>The following keys are rejected:<br><br>- Any label that expands to more than one file (mappings must be   one-to-one).<br><br>- Any label or file that was either not provided or explicitly   `exclude`d.<br><br>The following values result in undefined behavior:<br><br>- "" (the empty string)<br><br>- "."<br><br>- Anything containing ".."   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="pkg_files-strip_prefix"></a>strip_prefix |  What prefix of a file's path to discard prior to installation.<br><br>This specifies what prefix of an incoming file's path should not be included in the output package at after being appended to the install prefix (the `prefix` attribute).  Note that this is only applied to full directory names, see `strip_prefix` for more details.<br><br>Use the `strip_prefix` struct to define this attribute.  If this attribute is not specified, all directories will be stripped from all files prior to being included in packages (`strip_prefix.files_only()`).<br><br>If prefix stripping fails on any file provided in `srcs`, the build will fail.<br><br>Note that this only functions on paths that are known at analysis time.  Specifically, this will not consider directories within TreeArtifacts (directory outputs), or the directories themselves. See also #269.   | String | optional |  `"."`  |


<a id="pkg_mkdirs"></a>

## pkg_mkdirs

<pre>
pkg_mkdirs(<a href="#pkg_mkdirs-name">name</a>, <a href="#pkg_mkdirs-attributes">attributes</a>, <a href="#pkg_mkdirs-dirs">dirs</a>)
</pre>

Defines creation and ownership of directories in packages

Use this if:

1) You need to create an empty directory in your package.

2) Your package needs to explicitly own a directory, even if it already owns
   files in those directories.

3) You need nonstandard permissions (typically, not "0755") on a directory
   in your package.

For some package management systems (e.g. RPM), directory ownership (2) may
imply additional semantics.  Consult your package manager's and target
distribution's documentation for more details.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_mkdirs-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_mkdirs-attributes"></a>attributes |  Attributes to set on packaged directories.<br><br>Always use `pkg_attributes()` to set this rule attribute.<br><br>If not otherwise overridden, the directory's mode will be set to UNIX "0755", or the target platform's equivalent.<br><br>Consult the "Mapping Attributes" documentation in the rules_pkg reference for more details.   | String | optional |  `"{}"`  |
| <a id="pkg_mkdirs-dirs"></a>dirs |  Directory names to make within the package<br><br>If any part of the requested directory structure does not already exist within a package, the package builder will create it for you with a reasonable set of default permissions (typically `0755 root.root`).   | List of strings | required |  |


<a id="pkg_mklink_impl"></a>

## pkg_mklink_impl

<pre>
pkg_mklink_impl(<a href="#pkg_mklink_impl-name">name</a>, <a href="#pkg_mklink_impl-attributes">attributes</a>, <a href="#pkg_mklink_impl-link_name">link_name</a>, <a href="#pkg_mklink_impl-target">target</a>)
</pre>

Define a symlink  within packages

This rule results in the creation of a single link within a package.

Symbolic links specified by this rule may point at files/directories outside of the
package, or otherwise left dangling.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_mklink_impl-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_mklink_impl-attributes"></a>attributes |  Attributes to set on packaged symbolic links.<br><br>Always use `pkg_attributes()` to set this rule attribute.<br><br>Symlink permissions may have different meanings depending on your host operating system; consult its documentation for more details.<br><br>If not otherwise overridden, the link's mode will be set to UNIX "0777", or the target platform's equivalent.<br><br>Consult the "Mapping Attributes" documentation in the rules_pkg reference for more details.   | String | optional |  `"{}"`  |
| <a id="pkg_mklink_impl-link_name"></a>link_name |  Link "destination", a path within the package.<br><br>This is the actual created symbolic link.<br><br>If the directory structure provided by this attribute is not otherwise created when exist within the package when it is built, it will be created implicitly, much like with `pkg_files`.<br><br>This path may be prefixed or rooted by grouping or packaging rules.   | String | required |  |
| <a id="pkg_mklink_impl-target"></a>target |  Link "target", a path on the filesystem.<br><br>This is what the link "points" to, and may point to an arbitrary filesystem path, even relative paths.   | String | required |  |


<a id="pkg_attributes"></a>

## pkg_attributes

<pre>
pkg_attributes(<a href="#pkg_attributes-mode">mode</a>, <a href="#pkg_attributes-user">user</a>, <a href="#pkg_attributes-group">group</a>, <a href="#pkg_attributes-uid">uid</a>, <a href="#pkg_attributes-gid">gid</a>, <a href="#pkg_attributes-kwargs">kwargs</a>)
</pre>

Format attributes for use in package mapping rules.

If "mode" is not provided, it will default to the mapping rule's default
mode.  These vary per mapping rule; consult the respective documentation for
more details.

Not providing any of "user", "group", "uid", or "gid" will result in the package
builder choosing one for you.  The chosen value should not be relied upon.

Well-known attributes outside of the above are documented in the rules_pkg
reference.

This is the only supported means of passing in attributes to package mapping
rules (e.g. `pkg_files`).


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="pkg_attributes-mode"></a>mode |  string: UNIXy octal permissions, as a string.   |  `None` |
| <a id="pkg_attributes-user"></a>user |  string: Filesystem owning user name.   |  `None` |
| <a id="pkg_attributes-group"></a>group |  string: Filesystem owning group name.   |  `None` |
| <a id="pkg_attributes-uid"></a>uid |  int: Filesystem owning user id.   |  `None` |
| <a id="pkg_attributes-gid"></a>gid |  int: Filesystem owning group id.   |  `None` |
| <a id="pkg_attributes-kwargs"></a>kwargs |  any other desired attributes.   |  none |

**RETURNS**

A value usable in the "attributes" attribute in package mapping rules.


<a id="pkg_mklink"></a>

## pkg_mklink

<pre>
pkg_mklink(<a href="#pkg_mklink-name">name</a>, <a href="#pkg_mklink-link_name">link_name</a>, <a href="#pkg_mklink-target">target</a>, <a href="#pkg_mklink-attributes">attributes</a>, <a href="#pkg_mklink-src">src</a>, <a href="#pkg_mklink-kwargs">kwargs</a>)
</pre>

Create a symlink.

Wraps [pkg_mklink_impl](#pkg_mklink_impl)


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="pkg_mklink-name"></a>name |  target name   |  none |
| <a id="pkg_mklink-link_name"></a>link_name |  the path in the package that should point to the target.   |  none |
| <a id="pkg_mklink-target"></a>target |  target path that the link should point to.   |  none |
| <a id="pkg_mklink-attributes"></a>attributes |  file attributes.   |  `None` |
| <a id="pkg_mklink-src"></a>src |   -    |  `None` |
| <a id="pkg_mklink-kwargs"></a>kwargs |   -    |  none |


<a id="strip_prefix.files_only"></a>

## strip_prefix.files_only

<pre>
strip_prefix.files_only()
</pre>





<a id="strip_prefix.from_pkg"></a>

## strip_prefix.from_pkg

<pre>
strip_prefix.from_pkg(<a href="#strip_prefix.from_pkg-path">path</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="strip_prefix.from_pkg-path"></a>path |   -    |  `""` |


<a id="strip_prefix.from_root"></a>

## strip_prefix.from_root

<pre>
strip_prefix.from_root(<a href="#strip_prefix.from_root-path">path</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="strip_prefix.from_root-path"></a>path |   -    |  `""` |



<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rules to create RPM archives.

NOTE: this module is deprecated in favor of pkg/rpm_pfg.bzl. For more
information on the `pkg_filegroup` framework it uses, see pkg/mappings.bzl.

pkg_rpm() depends on the existence of an rpmbuild toolchain. Many users will
find to convenient to use the one provided with their system. To enable that
toolchain add the following stanza to WORKSPACE:

    # Find rpmbuild if it exists.
    load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")
    find_system_rpmbuild(name="rules_pkg_rpmbuild")

<a id="pkg_rpm"></a>

## pkg_rpm

<pre>
pkg_rpm(<a href="#pkg_rpm-name">name</a>, <a href="#pkg_rpm-data">data</a>, <a href="#pkg_rpm-architecture">architecture</a>, <a href="#pkg_rpm-changelog">changelog</a>, <a href="#pkg_rpm-debug">debug</a>, <a href="#pkg_rpm-release">release</a>, <a href="#pkg_rpm-release_file">release_file</a>, <a href="#pkg_rpm-rpmbuild_path">rpmbuild_path</a>,
        <a href="#pkg_rpm-source_date_epoch">source_date_epoch</a>, <a href="#pkg_rpm-source_date_epoch_file">source_date_epoch_file</a>, <a href="#pkg_rpm-spec_file">spec_file</a>, <a href="#pkg_rpm-version">version</a>, <a href="#pkg_rpm-version_file">version_file</a>)
</pre>

Legacy version

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="pkg_rpm-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="pkg_rpm-data"></a>data |  -   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="pkg_rpm-architecture"></a>architecture |  -   | String | optional |  `"all"`  |
| <a id="pkg_rpm-changelog"></a>changelog |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-debug"></a>debug |  -   | Boolean | optional |  `False`  |
| <a id="pkg_rpm-release"></a>release |  -   | String | optional |  `""`  |
| <a id="pkg_rpm-release_file"></a>release_file |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-rpmbuild_path"></a>rpmbuild_path |  -   | String | optional |  `""`  |
| <a id="pkg_rpm-source_date_epoch"></a>source_date_epoch |  -   | Integer | optional |  `0`  |
| <a id="pkg_rpm-source_date_epoch_file"></a>source_date_epoch_file |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="pkg_rpm-spec_file"></a>spec_file |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="pkg_rpm-version"></a>version |  -   | String | optional |  `""`  |
| <a id="pkg_rpm-version_file"></a>version_file |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
