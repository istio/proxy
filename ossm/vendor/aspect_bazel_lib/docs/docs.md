<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public API for docs helpers

<a id="stardoc_with_diff_test"></a>

## stardoc_with_diff_test

<pre>
stardoc_with_diff_test(<a href="#stardoc_with_diff_test-name">name</a>, <a href="#stardoc_with_diff_test-bzl_library_target">bzl_library_target</a>, <a href="#stardoc_with_diff_test-kwargs">kwargs</a>)
</pre>

Creates a stardoc target that can be auto-detected by update_docs to write the generated doc to the source tree and test that it's up to date.

This is helpful for minimizing boilerplate in repos with lots of stardoc targets.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="stardoc_with_diff_test-name"></a>name |  the name of the stardoc file to be written to the current source directory (.md will be appended to the name). Call bazel run on this target to update the file.   |  none |
| <a id="stardoc_with_diff_test-bzl_library_target"></a>bzl_library_target |  the label of the `bzl_library` target to generate documentation for   |  none |
| <a id="stardoc_with_diff_test-kwargs"></a>kwargs |  additional attributes passed to the stardoc() rule, such as for overriding the templates   |  none |


<a id="update_docs"></a>

## update_docs

<pre>
update_docs(<a href="#update_docs-name">name</a>, <a href="#update_docs-kwargs">kwargs</a>)
</pre>

Stamps an executable run for writing all stardocs declared with stardoc_with_diff_test to the source tree.

This is to be used in tandem with `stardoc_with_diff_test()` to produce a convenient workflow
for generating, testing, and updating all doc files as follows:

``` bash
bazel build //{docs_folder}/... && bazel test //{docs_folder}/... && bazel run //{docs_folder}:update
```

eg.

``` bash
bazel build //docs/... && bazel test //docs/... && bazel run //docs:update
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="update_docs-name"></a>name |  the name of executable target   |  `"update"` |
| <a id="update_docs-kwargs"></a>kwargs |  Other common named parameters such as `tags` or `visibility`   |  none |


