# js_image_layer faq

## why js_image_layer isn't like nodejs_image from rules_docker

That would require rules_js to take rules_docker as a dependency which would be not ideal given that rules_docker is in maintenance mode and [other](https://github.com/bazel-contrib/rules_oci) rulesets exists.
So instead of being full-fledged js_image rule that is specific to one container building ruleset, it tries to be not opinionated and work with any ruleset out there.

## why use custom builder script instead of using pkg_tar

There are two reasons why js_image_layer doesn't use pkg_tar.

1. It's a python dependency which we'd like to avoid
2. It doesn't work very well with `ctx.actions.symlink`. We needed to maintain a [patch](https://github.com/bazelbuild/rules_pkg/issues/115) to make it work with rules_js which is specific to rules_js and can't be pushed upstream.

Apart from that, `pkg_tar` is general and has more than needed here. maintaining 150LOC code is much easier than dealing with version skews and breaking changes in rules_pkg.

## why check-in the image builder as minified javascript

There are number of reasons for this. Most significant one is not transpiling from `ts` to `js` anytime there is a cache miss on userland or at all for that matter. next one is convenience running `.js` files directly without fetching necessary toolchain to fetch dependencies and transpilers just to run a `.ts` file.
