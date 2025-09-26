<!-- Generated with Stardoc: http://skydoc.bazel.build -->
# Settings

* [incompatible_flag](#incompatible_flag)
* [fail_when_enabled](#fail_when_enabled)

<a id="incompatible_flag"></a>

## incompatible_flag

<pre>
incompatible_flag(<a href="#incompatible_flag-name">name</a>, <a href="#incompatible_flag-issue">issue</a>)
</pre>

A rule defining an incompatible flag.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="incompatible_flag-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="incompatible_flag-issue"></a>issue |  The link to the github issue associated with this flag   | String | required |  |


