<!-- Generated with Stardoc: http://skydoc.bazel.build -->

npm_link_package rule

<a id="npm_link_package"></a>

## npm_link_package

<pre>
npm_link_package(<a href="#npm_link_package-name">name</a>, <a href="#npm_link_package-root_package">root_package</a>, <a href="#npm_link_package-link">link</a>, <a href="#npm_link_package-src">src</a>, <a href="#npm_link_package-deps">deps</a>, <a href="#npm_link_package-fail_if_no_link">fail_if_no_link</a>, <a href="#npm_link_package-auto_manual">auto_manual</a>, <a href="#npm_link_package-visibility">visibility</a>,
                 <a href="#npm_link_package-kwargs">kwargs</a>)
</pre>

"Links an npm package to node_modules if link is True.

When called at the root_package, a virtual store target is generated named `link__{bazelified_name}__store`.

When linking, a `{name}` target is generated which consists of the `node_modules/&lt;package&gt;` symlink and transitively
its virtual store link and the virtual store links of the transitive closure of deps.

When linking, `{name}/dir` filegroup is also generated that refers to a directory artifact can be used to access
the package directory for creating entry points or accessing files in the package.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="npm_link_package-name"></a>name |  The name of the link target to create if <code>link</code> is True. For first-party deps linked across a workspace, the name must match in all packages being linked as it is used to derive the virtual store link target name.   |  none |
| <a id="npm_link_package-root_package"></a>root_package |  the root package where the node_modules virtual store is linked to   |  <code>""</code> |
| <a id="npm_link_package-link"></a>link |  whether or not to link in this package If false, only the npm_package_store target will be created _if_ this is called in the <code>root_package</code>.   |  <code>True</code> |
| <a id="npm_link_package-src"></a>src |  the npm_package target to link; may only to be specified when linking in the root package   |  <code>None</code> |
| <a id="npm_link_package-deps"></a>deps |  list of npm_package_store; may only to be specified when linking in the root package   |  <code>{}</code> |
| <a id="npm_link_package-fail_if_no_link"></a>fail_if_no_link |  whether or not to fail if this is called in a package that is not the root package and <code>link</code> is False   |  <code>True</code> |
| <a id="npm_link_package-auto_manual"></a>auto_manual |  whether or not to automatically add a manual tag to the generated targets Links tagged "manual" dy default is desirable so that they are not built by <code>bazel build ...</code> if they are unused downstream. For 3rd party deps, this is particularly important so that 3rd party deps are not fetched at all unless they are used.   |  <code>True</code> |
| <a id="npm_link_package-visibility"></a>visibility |  the visibility of the link target   |  <code>["//visibility:public"]</code> |
| <a id="npm_link_package-kwargs"></a>kwargs |  see attributes of npm_package_store rule   |  none |

**RETURNS**

Label of the npm_link_package_store if created, else None


