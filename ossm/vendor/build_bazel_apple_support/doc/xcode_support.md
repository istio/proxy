# `xcode_support` Starlark Module

<!-- Generated file, do not edit directly. -->


A modules of helpers for rule authors to aid in writing rules that
need to change what they do based on attributes of the active Xcode.

To use these in your Starlark code, simply load the module; for example:

```build
load("@build_bazel_apple_support//lib:xcode_support.bzl", "xcode_support")
```

<!-- BEGIN_TOC -->
On this page:

  * [xcode_support.is_xcode_at_least_version](#xcode_support.is_xcode_at_least_version)
<!-- END_TOC -->


<a name="xcode_support.is_xcode_at_least_version"></a>
## xcode_support.is_xcode_at_least_version

<pre style="white-space: normal">
xcode_support.is_xcode_at_least_version(<a href="#xcode_support.is_xcode_at_least_version.xcode_config">xcode_config</a>, <a href="#xcode_support.is_xcode_at_least_version.version">version</a>)
</pre>

Returns True if Xcode version is at least a given version.

This method takes as input an `XcodeVersionConfig` provider, which can be obtained from the
`_xcode_config` attribute (e.g. `ctx.attr._xcode_config[apple_common.XcodeVersionConfig]`). This
provider should contain the Xcode version parameters with which this rule is being built with.
If you need to add this attribute to your rule implementation, please refer to
`apple_support.action_required_attrs()`.

<a name="xcode_support.is_xcode_at_least_version.arguments"></a>
### Arguments

<table class="params-table">
  <colgroup>
    <col class="col-param" />
    <col class="col-description" />
  </colgroup>
  <tbody>
    <tr id="xcode_support.is_xcode_at_least_version.xcode_config">
      <td><code>xcode_config</code></td>
      <td><p><code>Required</code></p><p>The XcodeVersionConfig provider from the <code>_xcode_config</code> attribute's value.</p></td>
    </tr>
    <tr id="xcode_support.is_xcode_at_least_version.version">
      <td><code>version</code></td>
      <td><p><code>Required</code></p><p>The minimum desired Xcode version, as a dotted version string.</p></td>
    </tr>
  </tbody>
</table>

<a name="xcode_support.is_xcode_at_least_version.returns"></a>
### Returns

True if the given `xcode_config` version at least as high as the requested version.


