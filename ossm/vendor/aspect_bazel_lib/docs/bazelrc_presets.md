<!-- Generated with Stardoc: http://skydoc.bazel.build -->

'Presets' for bazelrc

See https://docs.aspect.build/guides/bazelrc

<a id="write_aspect_bazelrc_presets"></a>

## write_aspect_bazelrc_presets

<pre>
write_aspect_bazelrc_presets(<a href="#write_aspect_bazelrc_presets-name">name</a>, <a href="#write_aspect_bazelrc_presets-presets">presets</a>, <a href="#write_aspect_bazelrc_presets-kwargs">kwargs</a>)
</pre>

Keeps your vendored copy of Aspect recommended `.bazelrc` presets up-to-date.

This macro uses a [write_source_files](https://docs.aspect.build/rules/aspect_bazel_lib/docs/write_source_files)
rule under the hood to keep your presets up-to-date.

By default all presets are vendored but this list can be customized using
the `presets` attribute.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="write_aspect_bazelrc_presets-name"></a>name |  a unique name for this target   |  none |
| <a id="write_aspect_bazelrc_presets-presets"></a>presets |  a list of preset names to keep up-to-date   |  `["bazel6", "bazel7", "ci", "convenience", "correctness", "debug", "java", "javascript", "performance"]` |
| <a id="write_aspect_bazelrc_presets-kwargs"></a>kwargs |  Additional arguments to pass to `write_source_files`   |  none |


