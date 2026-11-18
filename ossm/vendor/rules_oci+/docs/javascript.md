# Images containing JavaScript applications

We assume your application runs on the Node.js runtime, rather than alternatives like Deno.

Users are typically migrating from [nodejs_image](https://github.com/bazelbuild/rules_docker#nodejs_image) in rules_docker.

Typically to create layers for a JavaScript binary, you'll create a
[`js_binary`](https://github.com/aspect-build/rules_js/blob/main/docs/js_binary.md)
target, then pass it as the `binary` attribute to a
[`js_image_layer` rule](https://github.com/aspect-build/rules_js/blob/main/docs/js_image_layer.md).

This produces tar files which can be passed to the `tars` attribute of an `oci_image`.

## Example

A full example can be found in rules_js:
<https://github.com/aspect-build/rules_js/tree/main/e2e/js_image_oci>
