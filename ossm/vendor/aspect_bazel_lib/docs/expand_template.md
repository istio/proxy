<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public API for expand template

<a id="expand_template_rule"></a>

## expand_template_rule

<pre>
expand_template_rule(<a href="#expand_template_rule-name">name</a>, <a href="#expand_template_rule-data">data</a>, <a href="#expand_template_rule-out">out</a>, <a href="#expand_template_rule-is_executable">is_executable</a>, <a href="#expand_template_rule-stamp">stamp</a>, <a href="#expand_template_rule-stamp_substitutions">stamp_substitutions</a>, <a href="#expand_template_rule-substitutions">substitutions</a>,
                     <a href="#expand_template_rule-template">template</a>)
</pre>

Template expansion

This performs a simple search over the template file for the keys in substitutions,
and replaces them with the corresponding values.

Values may also use location templates as documented in
[expand_locations](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/expand_make_vars.md#expand_locations)
as well as [configuration variables](https://docs.bazel.build/versions/main/skylark/lib/ctx.html#var)
such as `$(BINDIR)`, `$(TARGET_CPU)`, and `$(COMPILATION_MODE)` as documented in
[expand_variables](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/expand_make_vars.md#expand_variables).

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="expand_template_rule-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="expand_template_rule-data"></a>data |  List of targets for additional lookup information.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="expand_template_rule-out"></a>out |  Where to write the expanded file.<br><br>If the `template` is a source file, then `out` defaults to be named the same as the template file and outputted to the same workspace-relative path. In this case there will be no pre-declared label for the output file. It can be referenced by the target label instead. This pattern is similar to `copy_to_bin` but with substitutions on the copy.<br><br>Otherwise, `out` defaults to `[name].txt`.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="expand_template_rule-is_executable"></a>is_executable |  Whether to mark the output file as executable.   | Boolean | optional |  `False`  |
| <a id="expand_template_rule-stamp"></a>stamp |  Whether to encode build information into the output. Possible values:<br><br>- `stamp = 1`: Always stamp the build information into the output, even in     [--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds.     This setting should be avoided, since it is non-deterministic.     It potentially causes remote cache misses for the target and     any downstream actions that depend on the result. - `stamp = 0`: Never stamp, instead replace build information by constant values.     This gives good build result caching. - `stamp = -1`: Embedding of build information is controlled by the     [--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.     Stamped targets are not rebuilt unless their dependencies change.   | Integer | optional |  `-1`  |
| <a id="expand_template_rule-stamp_substitutions"></a>stamp_substitutions |  Mapping of strings to substitutions.<br><br>There are overlaid on top of substitutions when stamping is enabled for the target.<br><br>Substitutions can contain $(execpath :target) and $(rootpath :target) expansions, $(MAKEVAR) expansions and {{STAMP_VAR}} expansions when stamping is enabled for the target.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="expand_template_rule-substitutions"></a>substitutions |  Mapping of strings to substitutions.<br><br>Substitutions can contain $(execpath :target) and $(rootpath :target) expansions, $(MAKEVAR) expansions and {{STAMP_VAR}} expansions when stamping is enabled for the target.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |
| <a id="expand_template_rule-template"></a>template |  The template file to expand.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


<a id="expand_template"></a>

## expand_template

<pre>
expand_template(<a href="#expand_template-name">name</a>, <a href="#expand_template-template">template</a>, <a href="#expand_template-kwargs">kwargs</a>)
</pre>

Wrapper macro for `expand_template_rule`.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="expand_template-name"></a>name |  name of resulting rule   |  none |
| <a id="expand_template-template"></a>template |  the label of a template file, or a list of strings which are lines representing the content of the template.   |  none |
| <a id="expand_template-kwargs"></a>kwargs |  other named parameters to `expand_template_rule`.   |  none |


