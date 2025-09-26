# Apple Support Starlark Modules and Rules

Apple Support provides a collection of Starlark helpers for rule authors
targeting Apple Platforms (and Xcode) as well as some Rules directly.

## Starlark Modules

<table class="table table-condensed table-bordered table-params">
  <thead>
    <tr>
      <th><code>.bzl</code> file</th>
      <th>Module</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td valign="top"><code>@build_bazel_apple_support//lib:apple_support.bzl</code></td>
      <td valign="top"><code><a href="apple_support.md">apple_support</a></code><br/></td>
    </tr>
    <tr>
      <td valign="top"><code>@build_bazel_apple_support//lib:xcode_support.bzl</code></td>
      <td valign="top"><code><a href="xcode_support.md">xcode_support</a></code><br/></td>
    </tr>
  </tbody>
</table>

## Rules

<table class="table table-condensed table-bordered table-params">
  <thead>
    <tr>
      <th><code>.bzl</code> file</th>
      <th>Rules</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td valign="top"><code>@build_bazel_apple_support//rules:apple_genrule.bzl</code></td>
      <td valign="top"><code><a href="rules.md#apple_genrule">apple_genrule</a></code><br/></td>
    </tr>
    <tr>
      <td valign="top"><code>@build_bazel_apple_support//rules:toolchain_substitution.bzl</code></td>
      <td valign="top"><code><a href="rules.md#toolchain_substitution">toolchain_substitution</a></code><br/></td>
    </tr>
    <tr>
      <td valign="top"><code>@build_bazel_apple_support//rules:universal_binary.bzl</code></td>
      <td valign="top"><code><a href="rules.md#universal_binary">universal_binary</a></code><br/></td>
    </tr>
  </tbody>
</table>
