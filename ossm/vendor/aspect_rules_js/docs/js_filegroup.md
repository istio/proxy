<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Helper rule to gather files from JsInfo providers of targets and provide them as default outputs

<a id="js_filegroup"></a>

## js_filegroup

<pre>
js_filegroup(<a href="#js_filegroup-name">name</a>, <a href="#js_filegroup-include_declarations">include_declarations</a>, <a href="#js_filegroup-include_npm_linked_packages">include_npm_linked_packages</a>, <a href="#js_filegroup-include_transitive_sources">include_transitive_sources</a>,
             <a href="#js_filegroup-srcs">srcs</a>)
</pre>

Gathers files from the JsInfo providers from targets in srcs and provides them as default outputs.

This helper rule is used by the `js_run_binary` macro.


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="js_filegroup-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="js_filegroup-include_declarations"></a>include_declarations |  When True, <code>declarations</code> and <code>transitive_declarations</code> from <code>JsInfo</code> providers in srcs targets are included in the default outputs of the target.<br><br>            Defaults to false since declarations are generally not needed at runtime and introducing them could slow down developer round trip             time due to having to generate typings on source file changes.   | Boolean | optional | <code>False</code> |
| <a id="js_filegroup-include_npm_linked_packages"></a>include_npm_linked_packages |  When True, files in <code>npm_linked_packages</code> and <code>transitive_npm_linked_packages</code> from <code>JsInfo</code> providers in srcs targets are included in the default outputs of the target.<br><br>            <code>transitive_files</code> from <code>NpmPackageStoreInfo</code> providers in data targets are also included in the default outputs of the target.   | Boolean | optional | <code>True</code> |
| <a id="js_filegroup-include_transitive_sources"></a>include_transitive_sources |  When True, <code>transitive_sources</code> from <code>JsInfo</code> providers in <code>srcs</code> targets are included in the default outputs of the target.   | Boolean | optional | <code>True</code> |
| <a id="js_filegroup-srcs"></a>srcs |  List of targets to gather files from.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional | <code>[]</code> |


