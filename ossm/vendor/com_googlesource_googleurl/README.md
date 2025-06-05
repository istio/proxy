# googleurl

This is a copy of [Chrome's URL parsing
library](https://cs.chromium.org/chromium/src/url/), adapted to work with
[Bazel](https://bazel.build/).  It is meant to be used by
[QUICHE](https://quiche.googlesource.com/quiche/+/refs/heads/master), but can be
also used by other projects that use Bazel.

In order to be used successfully, C++14 or later and `-fno-strict-aliasing`
compile flag are required.

For questions, contact <proto-quic@chromium.org>.

## Update Instructions

In order to update this copy to the latest version of googleurl in Chromium, run
the following commands in the root of the checkout:

1. `copybara copy.bara.sky import <path-to-chrome>/src --folder-dir .`
1. `bazel test --cxxopt="-std=c++14" //...`
   (C++14 is replacible with later C++ versions)
1. Fix all of the compilation errors, potentially modifying the BUILD files and
   the polyfill headers in `polyfill/` as appropriate.
1. Check the new version into Git.
