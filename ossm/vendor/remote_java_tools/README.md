This Java tools version was built from the bazel repository at commit hash 40ec2cd1a15d73473945475552ad1d0cb6d77927
using bazel version 7.1.1.
To build from source the same zip run the commands:

$ git clone https://github.com/bazelbuild/bazel.git
$ git checkout 40ec2cd1a15d73473945475552ad1d0cb6d77927
$ bazel build //src:java_tools.zip
