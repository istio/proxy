<meta name="robots" content="noindex,nofollow">
# rules_pkg - 0.4.0

<div class="toc">
  <h2>Rules</h2>
  <ul>
    <li><a href="#pkg_tar">pkg_tar</a></li>
    <li><a href="#pkg_zip">pkg_zip</a></li>
    <li><a href="#pkg_deb">pkg_deb</a></li>
    <li><a href="#pkg_rpm">pkg_rpm</a></li>
  </ul>

</div>

<a name="common"></a>
## Common Attributes

These attributes are used in several rules within this module.

**ATTRIBUTES**

| Name              | Description                                                                                                                                                                     | Type                                                               | Mandatory       | Default                                   |
| :-------------    | :-------------                                                                                                                                                                  | :-------------:                                                    | :-------------: | :-------------                            |
| out               | Name of the output file. This file will always be created and used to access the package content. If `package_file_name` is also specified, `out` will be a symlink.            | String                                                             | required        |                                           |
| package_file_name | The name of the file which will contain the package. The name may contain variables in the form `{var}`. The values for substitution are specified through `package_variables`. | String                                                             | optional        | package type specific                     |
| package_variables | A target that provides `PackageVariablesInfo` to substitute into `package_file_name`.                                                                                           | <a href="https://bazel.build/docs/build-ref.html#labels">Label</a> | optional        | None                                      |
| attributes        | Attributes to set on entities created within packages.  Not to be confused with bazel rule attributes.  See 'Mapping "Attributes"' below                                        | Undefined.                                                         | optional        | Varies.  Consult individual rule documentation for details. |

See
[examples/naming_package_files](https://github.com/bazelbuild/rules_pkg/tree/main/examples/naming_package_files)
for examples of how `out`, `package_file_name`, and `package_variables`
interact.

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

There are currently no other well-known attributes.

---

<a name="pkg_tar"></a>
## pkg_tar

```python
pkg_tar(name, extension, strip_prefix, package_dir, srcs,
        mode, modes, deps, symlinks, package_file_name, package_variables)
```

Creates a tar file from a list of inputs.

<table class="table table-condensed table-bordered table-params">
  <colgroup>
    <col class="col-param" />
    <col class="param-description" />
  </colgroup>
  <thead>
    <tr>
      <th colspan="2">Attributes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><code>name</code></td>
      <td>
        <code>Name, required</code>
        <p>A unique name for this rule.</p>
      </td>
    </tr>
    <tr>
      <td><code>extension</code></td>
      <td>
        <code>String, default to 'tar'</code>
        <p>
            The extension for the resulting tarball. The output
            file will be '<i>name</i>.<i>extension</i>'. This extension
            also decide on the compression: if set to <code>tar.gz</code>
            or <code>tgz</code> then gzip compression will be used and
            if set to <code>tar.bz2</code> or <code>tar.bzip2</code> then
            bzip2 compression will be used.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>strip_prefix</code></td>
      <td>
        <code>String, optional</code>
        <p>Root path of the files.</p>
        <p>
          The directory structure from the files is preserved inside the
          tarball but a prefix path determined by <code>strip_prefix</code>
          is removed from the directory structure. This path can
          be absolute from the workspace root if starting with a <code>/</code> or
          relative to the rule's directory. A relative path may start with "./"
          (or be ".") but cannot use ".." to go up level(s). By default, the
          <code>strip_prefix</code> attribute is unused and all files are supposed to have no
          prefix. A <code>strip_prefix</code> of "" (the empty string) means the
          same as the default.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>package_dir</code></td>
      <td>
        <code>String, optional</code>
        <p>Target directory.</p>
        <p>
          The directory in which to expand the specified files, defaulting to '/'.
          Only makes sense accompanying files.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>srcs</code></td>
      <td>
        <code>List of files, optional</code>
        <p>File to add to the layer.</p>
        <p>
          A list of files that should be included in the archive.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>mode</code></td>
      <td>
        <code>String, default to 0555</code>
        <p>
          Set the mode of files added by the <code>srcs</code> attribute.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>mtime</code></td>
      <td>
        <code>int, seconds since Jan 1, 1970, default to -1 (ignored)</code>
        <p>
          Set the modification time of files added by the <code>srcs</code> attribute.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>portable_mtime</code></td>
      <td>
        <code>bool, default True</code>
        <p>
          Set the modification time of files added by the <code>srcs</code> attribute
          to a 2000-01-01.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>modes</code></td>
      <td>
        <code>Dictionary, default to '{}'</code>
        <p>
          A string dictionary to change default mode of specific files from
          <code>srcs</code>. Each key should be a path to a file before
          appending the prefix <code>package_dir</code> and the corresponding
          value the octal permission of to apply to the file.
        </p>
        <p>
          <code>
          modes = {
           "tools/py/2to3.sh": "0755",
           ...
          },
          </code>
        </p>
      </td>
    </tr>
    <tr>
      <td><code>owner</code></td>
      <td>
        <code>String, default to '0.0'</code>
        <p>
          <code>UID.GID</code> to set the default numeric owner for all files
          provided in <code>srcs</code>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>owners</code></td>
      <td>
        <code>Dictionary, default to '{}'</code>
        <p>
          A string dictionary to change default owner of specific files from
          <code>srcs</code>. Each key should be a path to a file before
          appending the prefix <code>package_dir</code> and the corresponding
          value the <code>UID.GID</code> numeric string for the owner of the
          file. When determining owner ids, this attribute is looked first then
          <code>owner</code>.
        </p>
        <p>
          <code>
          owners = {
           "tools/py/2to3.sh": "42.24",
           ...
          },
          </code>
        </p>
      </td>
    </tr>
    <tr>
      <td><code>ownername</code></td>
      <td>
        <code>String, optional</code>
        <p>
          <code>username.groupname</code> to set the default owner for all files
          provided in <code>srcs</code> (by default there is no owner names).
        </p>
      </td>
    </tr>
    <tr>
      <td><code>ownernames</code></td>
      <td>
        <code>Dictionary, default to '{}'</code>
        <p>
          A string dictionary to change default owner of specific files from
          <code>srcs</code>. Each key should be a path to a file before
          appending the prefix <code>package_dir</code> and the corresponding
          value the <code>username.groupname</code> string for the owner of the
          file. When determining ownernames, this attribute is looked first then
          <code>ownername</code>.
        </p>
        <p>
          <code>
          owners = {
           "tools/py/2to3.sh": "leeroy.jenkins",
           ...
          },
          </code>
        </p>
      </td>
    </tr>
    <tr>
      <td><code>deps</code></td>
      <td>
        <code>List of labels, optional</code>
        <p>Tar files to extract and include in this tar package.</p>
        <p>
          A list of tarball labels to merge into the output tarball.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>symlinks</code></td>
      <td>
        <code>Dictionary, optional</code>
        <p>Symlinks to create in the output tarball.</p>
        <p>
          <code>
          symlinks = {
           "/path/to/link": "/path/to/target",
           ...
          },
          </code>
        </p>
      </td>
    </tr>
    <tr>
      <td><code>remap_paths</code></td>
      <td>
        <code>Dictionary, optional</code>
        <p>Source path prefixes to remap in the tarfile.</p>
        <p>
          <code>
          remap_paths = {
           "original/path/prefix": "replaced/path",
           ...
          },
          </code>
        </p>
      </td>
    </tr>
    <tr>
      <td><code>package_file_name</code></td>
      <td>See <a href="#common">Common Attributes</a></td>
    </tr>
    <tr>
      <td><code>package_variables</code></td>
      <td>See <a href="#common">Common Attributes</a></td>
    </tr>
  </tbody>
</table>

<a name="pkg_zip"></a>
## pkg_zip

```python
pkg_zip(name, extension, package_dir, srcs, timestamp, package_file_name,
package_variables)
```

Creates a zip file from a list of inputs.

<table class="table table-condensed table-bordered table-params">
  <colgroup>
    <col class="col-param" />
    <col class="param-description" />
  </colgroup>
  <thead>
    <tr>
      <th colspan="2">Attributes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><code>name</code></td>
      <td>
        <code>Name, required</code>
        <p>A unique name for this rule.</p>
      </td>
    </tr>
    <tr>
      <td><code>extension</code></td>
      <td>
        <code>String, default to 'zip'</code>
        <p>
            <b>Deprecated. Use <code>out</code> or <code>package_file_name</code> to specify the output file name.</b>
            The extension for the resulting zipfile. The output
            file will be '<i>name</i>.<i>extension</i>'.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>package_dir</code></td>
      <td>
        <code>String, default to '/'</code>
        <p>Target directory inside zip.</p>
        <p>
          The prefix of all paths in the zip.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>srcs</code></td>
      <td>
        <code>List of files, optional</code>
        <p>File to add to the layer.</p>
        <p>
          A list of files that should be included in the archive.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>timestamp</code></td>
      <td>
        <code>Integer, default to 315532800</code>
        <p>
          The time to use for every file in the zip, expressed as seconds since
          Unix Epoch, RFC 3339.
        </p>
        <p>
          Due to limitations in the format of zip files, values before
          Jan 1, 1980 will be rounded up and the precision in the zip file is
          limited to a granularity of 2 seconds.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>package_file_name</code></td>
      <td>See <a href="#common">Common Attributes</a></td>
    </tr>
    <tr>
      <td><code>package_variables</code></td>
      <td>See <a href="#common">Common Attributes</a></td>
    </tr>
  </tbody>
</table>

<a name="pkg_deb"></a>
### pkg_deb

```python
pkg_deb(name, data, package, architecture, maintainer, preinst, postinst, prerm, postrm,
        version, version_file, description, description_file, built_using, built_using_file,
        priority, section, homepage, depends, suggests, enhances, breaks, conflicts,
        predepends, recommends, replaces, package_file_name, package_variables)
```

Create a debian package. See <a
href="http://www.debian.org/doc/debian-policy/ch-controlfields.html">http://www.debian.org/doc/debian-policy/ch-controlfields.html</a>
for more details on this.

<table class="table table-condensed table-bordered table-params">
  <colgroup>
    <col class="col-param" />
    <col class="param-description" />
  </colgroup>
  <thead>
    <tr>
      <th colspan="2">Attributes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><code>name</code></td>
      <td>
        <code>Name, required</code>
        <p>A unique name for this rule.</p>
      </td>
    </tr>
    <tr>
      <td><code>data</code></td>
      <td>
        <code>File, required</code>
        <p>
          A tar file that contains the data for the debian package (basically
          the list of files that will be installed by this package).
        </p>
      </td>
    </tr>
    <tr>
      <td><code>package</code></td>
      <td>
        <code>String, required</code>
        <p>The name of the package.</p>
      </td>
    </tr>
    <tr>
      <td><code>architecture</code></td>
      <td>
        <code>String, default to 'all'</code>
        <p>The architecture that this package target.</p>
        <p>
          See <a href="http://www.debian.org/ports/">http://www.debian.org/ports/</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>maintainer</code></td>
      <td>
        <code>String, required</code>
        <p>The maintainer of the package.</p>
      </td>
    </tr>
    <tr>
      <td><code>preinst</code>, <code>postinst</code>, <code>prerm</code> and <code>postrm</code></td>
      <td>
        <code>Files, optional</code>
        <p>
          Respectively, the pre-install, post-install, pre-remove and
          post-remove scripts for the package.
        </p>
        <p>
          See <a href="http://www.debian.org/doc/debian-policy/ch-maintainerscripts.html">http://www.debian.org/doc/debian-policy/ch-maintainerscripts.html</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>config</code></td>
      <td>
        <code>File, optional</code>
        <p>
          config file used for debconf integration.
        </p>
        <p>
          See <a href="https://www.debian.org/doc/debian-policy/ch-binary.html#prompting-in-maintainer-scripts">https://www.debian.org/doc/debian-policy/ch-binary.html#prompting-in-maintainer-scripts</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>templates</code></td>
      <td>
        <code>File, optional</code>
        <p>
          templates file used for debconf integration.
        </p>
        <p>
          See <a href="https://www.debian.org/doc/debian-policy/ch-binary.html#prompting-in-maintainer-scripts">https://www.debian.org/doc/debian-policy/ch-binary.html#prompting-in-maintainer-scripts</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>triggers</code></td>
      <td>
        <code>File, optional</code>
        <p>
          triggers file for configuring installation events exchanged by packages.
        </p>
        <p>
          See <a href="https://wiki.debian.org/DpkgTriggers">https://wiki.debian.org/DpkgTriggers</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>conffiles</code>, <code>conffiles_file</code></td>
      <td>
        <code>String list or File, optional</code>
        <p>
          The list of conffiles or a file containing one conffile per
          line. Each item is an absolute path on the target system
          where the deb is installed.
        </p>
        <p>
          See <a href="https://www.debian.org/doc/debian-policy/ch-files.html#s-config-files">https://www.debian.org/doc/debian-policy/ch-files.html#s-config-files</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>version</code>, <code>version_file</code></td>
      <td>
        <code>String or File, required</code>
        <p>
          The package version provided either inline (with <code>version</code>)
          or from a file (with <code>version_file</code>).
        </p>
      </td>
    </tr>
    <tr>
      <td><code>description</code>, <code>description_file</code></td>
      <td>
        <code>String or File, required</code>
        <p>
          The package description provided either inline (with <code>description</code>)
          or from a file (with <code>description_file</code>).
        </p>
      </td>
    </tr>
    <tr>
      <td><code>built_using</code>, <code>built_using_file</code></td>
      <td>
        <code>String or File</code>
        <p>
          The tool that were used to build this package provided either inline
          (with <code>built_using</code>) or from a file (with <code>built_using_file</code>).
        </p>
      </td>
    </tr>
    <tr>
      <td><code>priority</code></td>
      <td>
        <code>String, default to 'optional'</code>
        <p>The priority of the package.</p>
        <p>
          See <a href="http://www.debian.org/doc/debian-policy/ch-archive.html#s-priorities">http://www.debian.org/doc/debian-policy/ch-archive.html#s-priorities</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>section</code></td>
      <td>
        <code>String, default to 'contrib/devel'</code>
        <p>The section of the package.</p>
        <p>
          See <a href="http://www.debian.org/doc/debian-policy/ch-archive.html#s-subsections">http://www.debian.org/doc/debian-policy/ch-archive.html#s-subsections</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>homepage</code></td>
      <td>
        <code>String, optional</code>
        <p>The homepage of the project.</p>
      </td>
    </tr>
    <tr>
      <td>
        <code>breaks</code>, <code>depends</code>, <code>suggests</code>,
        <code>enhances</code>, <code>conflicts</code>, <code>predepends</code>,
        <code>recommends</code>, <code>replaces</code> and <code>provides</code>.
      </td>
      <td>
        <code>String list, optional</code>
        <p>The list of dependencies in the project.</p>
        <p>
          See <a href="http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps">http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>package_file_name</code></td>
      <td>See <a href="#common">Common Attributes</a>
      Default: "%{package}-%{version}-%{architecture}.deb"
      </td>
    </tr>
    <tr>
      <td><code>package_variables</code></td>
      <td>See <a href="#common">Common Attributes</a></td>
    </tr>
  </tbody>
</table>

<a name="pkg_rpm"></a>
### pkg_rpm

```python
pkg_rpm(name, spec_file, architecture, version, version_file, changelog, data)
```

Create an RPM package. See <a
href="http://rpm.org/documentation.html">http://rpm.org/documentation.html</a>
for more details on this.

<table class="table table-condensed table-bordered table-params">
  <colgroup>
    <col class="col-param" />
    <col class="param-description" />
  </colgroup>
  <thead>
    <tr>
      <th colspan="2">Attributes</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><code>name</code></td>
      <td>
        <code>Name, required</code>
        <p>A unique name for this rule. Used to name the output package.</p>
      </td>
    </tr>
    <tr>
      <td><code>spec_file</code></td>
      <td>
        <code>File, required</code>
        <p>The RPM specification file used to generate the package.</p>
        <p>
          See <a href="http://ftp.rpm.org/max-rpm/s1-rpm-build-creating-spec-file.html">http://ftp.rpm.org/max-rpm/s1-rpm-build-creating-spec-file.html</a>.
        </p>
      </td>
    </tr>
    <tr>
      <td><code>architecture</code></td>
      <td>
        <code>String, default to 'all'</code>
        <p>The architecture that this package target.</p>
      </td>
    </tr>
    <tr>
      <td><code>version</code>, <code>version_file</code></td>
      <td>
        <code>String or File, required</code>
        <p>
          The package version provided either inline (with <code>version</code>)
          or from a file (with <code>version_file</code>).
        </p>
      </td>
    </tr>
    <tr>
      <td><code>data</code></td>
      <td>
        <code>Files, required</code>
        <p>
          Files to include in the generated package.
        </p>
      </td>
    </tr>
  </tbody>
</table>
