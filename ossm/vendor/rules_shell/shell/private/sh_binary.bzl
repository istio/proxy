# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""sh_binary rule definition."""

load(":sh_executable.bzl", "make_sh_executable_rule")

# For doc generation only.
visibility("public")

sh_binary = make_sh_executable_rule(
    doc = """
<p>
  The <code>sh_binary</code> rule is used to declare executable shell scripts.
  (<code>sh_binary</code> is a misnomer: its outputs aren't necessarily binaries.) This rule ensures
  that all dependencies are built, and appear in the <code>runfiles</code> area at execution time.
  We recommend that you name your <code>sh_binary()</code> rules after the name of the script minus
  the extension (e.g. <code>.sh</code>); the rule name and the file name must be distinct.
  <code>sh_binary</code> respects shebangs, so any available interpreter may be used (eg.
  <code>#!/bin/zsh</code>)
</p>
<h4 id="sh_binary_examples">Example</h4>
<p>For a simple shell script with no dependencies and some data files:
</p>
<pre class="code">
sh_binary(
    name = "foo",
    srcs = ["foo.sh"],
    data = glob(["datafiles/*.txt"]),
)
</pre>
""",
    executable = True,
)
