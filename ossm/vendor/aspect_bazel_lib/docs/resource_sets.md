<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Utilities for rules that expose resource_set on ctx.actions.run[_shell]

Workaround for https://github.com/bazelbuild/bazel/issues/15187

Note, this workaround only provides some fixed values for either CPU or Memory.

Rule authors who are ALSO the BUILD author might know better, and can
write custom resource_set functions for use within their own repository.
This seems to be the use case that Google engineers imagined.

<a id="resource_set"></a>

## resource_set

<pre>
resource_set(<a href="#resource_set-attr">attr</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="resource_set-attr"></a>attr |  <p align="center"> - </p>   |  none |


