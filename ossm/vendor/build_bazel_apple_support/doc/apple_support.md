# `apple_support` Starlark Module

<!-- Generated file, do not edit directly. -->


A module of helpers for rule authors to aid in writing actions that
target Apple platforms.

To use these in your Starlark code, simply load the module; for example:

```build
load("@build_bazel_apple_support//lib:apple_support.bzl", "apple_support")
```

<!-- BEGIN_TOC -->
On this page:

  * [apple_support.action_required_attrs](#apple_support.action_required_attrs)
  * [apple_support.action_required_env](#apple_support.action_required_env)
  * [apple_support.action_required_execution_requirements](#apple_support.action_required_execution_requirements)
  * [apple_support.path_placeholders.platform_frameworks](#apple_support.path_placeholders.platform_frameworks)
  * [apple_support.path_placeholders.sdkroot](#apple_support.path_placeholders.sdkroot)
  * [apple_support.path_placeholders.xcode](#apple_support.path_placeholders.xcode)
  * [apple_support.run](#apple_support.run)
  * [apple_support.run_shell](#apple_support.run_shell)
<!-- END_TOC -->


<a name="apple_support.action_required_attrs"></a>
## apple_support.action_required_attrs

<pre style="white-space: normal">
apple_support.action_required_attrs()
</pre>

Returns a dictionary with required attributes for registering actions on Apple platforms.

This method adds private attributes which should not be used outside of the apple_support
codebase. It also adds the following attributes which are considered to be public for rule
maintainers to use:

 * `_xcode_config`: Attribute that references a target containing the single
   `apple_common.XcodeVersionConfig` provider. This provider can be used to inspect Xcode-related
   properties about the Xcode being used for the build, as specified with the `--xcode_version`
   Bazel flag. The most common way to retrieve this provider is:
   `ctx.attr._xcode_config[apple_common.XcodeVersionConfig]`.

The returned `dict` can be added to the rule's attributes using Skylib's `dicts.add()` method.

<a name="apple_support.action_required_attrs.returns"></a>
### Returns

A `dict` object containing attributes to be added to rule implementations.

<a name="apple_support.action_required_env"></a>
## apple_support.action_required_env

<pre style="white-space: normal">
apple_support.action_required_env(<a href="#apple_support.action_required_env.ctx">ctx</a>, *,
<a href="#apple_support.action_required_env.xcode_config">xcode_config</a>,
<a href="#apple_support.action_required_env.apple_fragment">apple_fragment</a>)
</pre>

Returns a dictionary with the environment variables required for Xcode path resolution.

In most cases, you should _not_ use this API. It exists solely for using it on test rules,
where the test action registration API is not available in Starlark.

To use these environment variables for a test, your test rule needs to propagate the
`testing.TestEnvironment` provider, which takes a dictionary with environment variables to set
during the test execution.

<a name="apple_support.action_required_env.arguments"></a>
### Arguments

<table class="params-table">
  <colgroup>
    <col class="col-param" />
    <col class="col-description" />
  </colgroup>
  <tbody>
    <tr id="apple_support.action_required_env.ctx">
      <td><code>ctx</code></td>
      <td><p><code>Optional; default is None</code></p><p>The context of the rule registering the
            action. Required if <code>xcode_config</code> and <code>apple_fragment</code> are not
            provided. Deprecated.</p></td>
    </tr>
    <tr id="apple_support.action_required_env.xcode_config">
      <td><code>xcode_config</code></td>
      <td><p><code>Optional; default is None</code></p><p>The
            <code>apple_common.XcodeVersionConfig</code> provider as found in the current rule or
            aspect's context. Typically from
            <code>ctx.attr._xcode_config[apple_common.XcodeVersionConfig]</code>. Required if
            <code>ctx</code> is not given.</p></td>
    </tr>
    <tr id="apple_support.action_required_env.apple_fragment">
      <td><code>apple_fragment</code></td>
      <td><p><code>Optional; default is None</code></p><p>A reference to the apple fragment.
            Typically from <code>ctx.fragments.apple</code>. Required if <code>ctx</code> is not
            given.</p></td>
    </tr>
  </tbody>
</table>

<a name="apple_support.action_required_env.returns"></a>
### Returns

A dictionary with environment variables required for Xcode path resolution.

<a name="apple_support.action_required_execution_requirements"></a>
## apple_support.action_required_execution_requirements

<pre style="white-space: normal">
apple_support.action_required_execution_requirements(<a href="#apple_support.action_required_execution_requirements.ctx">ctx</a>, *, <a href="#apple_support.action_required_execution_requirements.xcode_config">xcode_config</a>)
</pre>

Returns a dictionary with the execution requirements for running actions on Apple platforms.

In most cases, you should _not_ use this API. It exists solely for using it on test rules,
where the test action registration API is not available in Starlark.

To use these environment variables for a test, your test rule needs to propagate the
`testing.TestExecution` provider, which takes a dictionary with execution requirements for the
test action.

<a name="apple_support.action_required_execution_requirements.arguments"></a>
### Arguments

<table class="params-table">
  <colgroup>
    <col class="col-param" />
    <col class="col-description" />
  </colgroup>
  <tbody>
    <tr id="apple_support.action_required_execution_requirements.ctx">
      <td><code>ctx</code></td>
      <td><p><code>Optional; default is None</code></p><p>The context of the rule registering the
            action. Required if <code>xcode_config</code> is not provided. Deprecated.</p></td>
    </tr>
    <tr id="apple_support.action_required_execution_requirements.xcode_config">
      <td><code>xcode_config</code></td>
      <td><p><code>Optional; default is None</code></p><p>The
            <code>apple_common.XcodeVersionConfig</code> provider as found in the current rule or
            aspect's context. Typically from
            <code>ctx.attr._xcode_config[apple_common.XcodeVersionConfig]</code>. Required if
            <code>ctx</code> is not given.</p></td>
    </tr>
  </tbody>
</table>

<a name="apple_support.action_required_execution_requirements.returns"></a>
### Returns

A dictionary with execution requirements for running actions on Apple platforms.


<a name="apple_support.path_placeholders.platform_frameworks"></a>
## apple_support.path_placeholders.platform_frameworks

<pre style="white-space: normal">
apple_support.path_placeholders.platform_frameworks(<a href="#apple_support.path_placeholders.platform_frameworks.ctx">ctx</a>, *, <a href="#apple_support.path_placeholders.platform_frameworks.apple_fragment">apple_fragment</a>)
</pre>

Returns the platform's frameworks directory, anchored to the Xcode path placeholder.

<a name="apple_support.path_placeholders.platform_frameworks.arguments"></a>
### Arguments

<table class="params-table">
  <colgroup>
    <col class="col-param" />
    <col class="col-description" />
  </colgroup>
  <tbody>
    <tr id="apple_support.path_placeholders.platform_frameworks.ctx">
      <td><code>ctx</code></td>
      <td><p><code>Optional; default is None</code></p><p>The context of the rule registering the
            action. Required if <code>apple_fragment</code> is not provided. Deprecated.</p></td>
    </tr>
    <tr id="apple_support.path_placeholders.platform_frameworks.apple_fragment">
      <td><code>apple_fragment</code></td>
      <td><p><code>Optional; default is None</code></p><p>A reference to the apple fragment.
            Typically from <code>ctx.fragments.apple</code>. Required if <code>ctx</code> is not
            given.</p></td>
    </tr>
  </tbody>
</table>

<a name="apple_support.path_placeholders.platform_frameworks.returns"></a>
### Returns

Returns a string with the platform's frameworks directory, anchored to the Xcode path
placeholder.

<a name="apple_support.path_placeholders.sdkroot"></a>
## apple_support.path_placeholders.sdkroot

<pre style="white-space: normal">
apple_support.path_placeholders.sdkroot()
</pre>

Returns a placeholder value to be replaced with SDKROOT during action execution.

In order to get this values replaced, you'll need to use the `apple_support.run()` API by
setting the `xcode_path_resolve_level` argument to either the
`apple_support.xcode_path_resolve_level.args` or
`apple_support.xcode_path_resolve_level.args_and_files` value.

<a name="apple_support.path_placeholders.sdkroot.returns"></a>
### Returns

Returns a placeholder value to be replaced with SDKROOT during action execution.

<a name="apple_support.path_placeholders.xcode"></a>
## apple_support.path_placeholders.xcode

<pre style="white-space: normal">
apple_support.path_placeholders.xcode()
</pre>

Returns a placeholder value to be replaced with DEVELOPER_DIR during action execution.

In order to get this values replaced, you'll need to use the `apple_support.run()` API by
setting the `xcode_path_resolve_level` argument to either the
`apple_support.xcode_path_resolve_level.args` or
`apple_support.xcode_path_resolve_level.args_and_files` value.

<a name="apple_support.path_placeholders.xcode.returns"></a>
### Returns

Returns a placeholder value to be replaced with DEVELOPER_DIR during action execution.


<a name="apple_support.run"></a>
## apple_support.run

<pre style="white-space: normal">
apple_support.run(<a href="#apple_support.run.ctx">ctx</a>, <a href="#apple_support.run.xcode_path_resolve_level">xcode_path_resolve_level</a>, *, <a href="#apple_support.run.actions">actions</a>, <a href="#apple_support.run.xcode_config">xcode_config</a>, <a href="#apple_support.run.apple_fragment">apple_fragment</a>, <a href="#apple_support.run.xcode_path_wrapper">xcode_path_wrapper</a>, <a href="#apple_support.run.**kwargs">**kwargs</a>)
</pre>

Registers an action to run on an Apple machine.

In order to use `apple_support.run()`, you'll need to modify your rule definition to add the
following:

  * `fragments = ["apple"]`
  * Add the `apple_support.action_required_attrs()` attributes to the `attrs` dictionary. This
    can be done using the `dicts.add()` method from Skylib.

This method registers an action to run on an Apple machine, configuring it to ensure that the
`DEVELOPER_DIR` and `SDKROOT` environment variables are set.

If the `xcode_path_resolve_level` is enabled, this method will replace the given `executable`
with a wrapper script that will replace all instances of the `__BAZEL_XCODE_DEVELOPER_DIR__` and
`__BAZEL_XCODE_SDKROOT__` placeholders in the given arguments with the values of `DEVELOPER_DIR`
and `SDKROOT`, respectively.

In your rule implementation, you can use references to Xcode through the
`apple_support.path_placeholders` API, which in turn uses the placeholder values as described
above. The available APIs are:

  * `apple_support.path_placeholders.xcode()`: Returns a reference to the Xcode.app
    installation path.
  * `apple_support.path_placeholders.sdkroot()`: Returns a reference to the SDK root path.
  * `apple_support.path_placeholders.platform_frameworks(ctx)`: Returns the Frameworks path
    within the Xcode installation, for the requested platform.

If the `xcode_path_resolve_level` value is:

  * `apple_support.xcode_path_resolve_level.none`: No processing will be done to the given
    `arguments`.
  * `apple_support.xcode_path_resolve_level.args`: Only instances of the placeholders in the
     argument strings will be replaced.
  * `apple_support.xcode_path_resolve_level.args_and_files`: Instances of the placeholders in
     the arguments strings and instances of the placeholders within response files (i.e. any
     path argument beginning with `@`) will be replaced.

<a name="apple_support.run.arguments"></a>
### Arguments

<table class="params-table">
  <colgroup>
    <col class="col-param" />
    <col class="col-description" />
  </colgroup>
  <tbody>
    <tr id="apple_support.run.ctx">
      <td><code>ctx</code></td>
      <td><p><code>Optional; default is None</code></p><p>The context of the rule registering the
            action. Required if <code>xcode_config</code> and <code>apple_fragment</code> are not
            provided. Deprecated.</p></td>
    </tr>
    <tr id="apple_support.run.xcode_path_resolve_level">
      <td><code>xcode_path_resolve_level</code></td>
      <td><p><code>Optional; default is apple_support.xcode_path_resolve_level.none</code></p>
      <p>The level of Xcode path replacement required for the action.</p></td>
    </tr>
    <tr id="apple_support.run.actions">
      <td><code>actions</code></td>
      <td><p><code>Optional; default is None</code></p><p>The actions provider from
            <code>ctx.actions</code>. Required if <code>ctx</code> is not given.</p></td>
    </tr>
    <tr id="apple_support.run.xcode_config">
      <td><code>xcode_config</code></td>
      <td><p><code>Optional; default is None</code></p><p>The
            <code>apple_common.XcodeVersionConfig</code> provider as found in the current rule or
            aspect's context. Typically from
            <code>ctx.attr._xcode_config[apple_common.XcodeVersionConfig]</code>. Required if
            <code>ctx</code> is not given.</p></td>
    </tr>
    <tr id="apple_support.run.apple_fragment">
      <td><code>apple_fragment</code></td>
      <td><p><code>Optional; default is None</code></p><p>A reference to the apple fragment.
            Typically from <code>ctx.fragments.apple</code>. Required if <code>ctx</code> is not
            given.</p></td>
    </tr>
    <tr id="apple_support.run.xcode_path_wrapper">
      <td><code>xcode_path_wrapper</code></td>
      <td><p><code>Optional; default is None</code></p><p>The Xcode path wrapper script. Required
            if <code>ctx</code> is not given and <code>xcode_path_resolve_level</code> is not
            <code>apple_support.xcode_path_resolve_level.none</code>.</p></td>
    </tr>
    <tr id="apple_support.run.**kwargs">
      <td><code>**kwargs</code></td>
      <td><p>See <code>ctx.actions.run</code> for the rest of the available arguments.</p></td>
    </tr>
  </tbody>
</table>

<a name="apple_support.run_shell"></a>
## apple_support.run_shell

<pre style="white-space: normal">
apple_support.run_shell(<a href="#apple_support.run_shell.ctx">ctx</a>, *, <a href="#apple_support.run_shell.actions">actions</a>, <a href="#apple_support.run_shell.xcode_config">xcode_config</a>, <a href="#apple_support.run_shell.apple_fragment">apple_fragment</a>, <a href="#apple_support.run_shell.**kwargs">**kwargs</a>)
</pre>

Registers a shell action to run on an Apple machine.

In order to use `apple_support.run_shell()`, you'll need to modify your rule definition to add
the following:

  * `fragments = ["apple"]`
  * Add the `apple_support.action_required_attrs()` attributes to the `attrs` dictionary. This
    can be done using the `dicts.add()` method from Skylib.

This method registers an action to run on an Apple machine, configuring it to ensure that the
`DEVELOPER_DIR` and `SDKROOT` environment variables are set.

`run_shell` does not support placeholder substitution. To achieve placeholder substitution,
please use `run` instead.

<a name="apple_support.run_shell.arguments"></a>
### Arguments

<table class="params-table">
  <colgroup>
    <col class="col-param" />
    <col class="col-description" />
  </colgroup>
  <tbody>
    <tr id="apple_support.run_shell.ctx">
      <td><code>ctx</code></td>
      <td><p><code>Optional; default is None</code></p><p>The context of the rule registering the
            action. Required if <code>xcode_config</code> and <code>apple_fragment</code> are not
            provided. Deprecated.</p></td>
    </tr>
    <tr id="apple_support.run_shell.actions">
      <td><code>actions</code></td>
      <td><p><code>Optional; default is None</code></p><p>The actions provider from
            <code>ctx.actions</code>. Required if <code>ctx</code> is not given.</p></td>
    </tr>
    <tr id="apple_support.run_shell.xcode_config">
      <td><code>xcode_config</code></td>
      <td><p><code>Optional; default is None</code></p><p>The
            <code>apple_common.XcodeVersionConfig</code> provider as found in the current rule or
            aspect's context. Typically from
            <code>ctx.attr._xcode_config[apple_common.XcodeVersionConfig]</code>. Required if
            <code>ctx</code> is not given.</p></td>
    </tr>
    <tr id="apple_support.run_shell.apple_fragment">
      <td><code>apple_fragment</code></td>
      <td><p><code>Optional; default is None</code></p><p>A reference to the apple fragment.
            Typically from <code>ctx.fragments.apple</code>. Required if <code>ctx</code> is not
            given.</p></td>
    </tr>
    <tr id="apple_support.run_shell.**kwargs">
      <td><code>**kwargs</code></td>
      <td><p>See <code>ctx.actions.run_shell</code> for the rest of the available arguments.</p></td>
    </tr>
  </tbody>
</table>




