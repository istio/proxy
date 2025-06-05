# Copyright 2019 The Bazel Authors. All rights reserved.
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

"Helpers for copy rules"

# Hints for Bazel spawn strategy
COPY_EXECUTION_REQUIREMENTS = {
    # ----------------+-----------------------------------------------------------------------------
    # no-remote       | Prevents the action or test from being executed remotely or cached remotely.
    #                 | This is equivalent to using both `no-remote-cache` and `no-remote-exec`.
    # ----------------+-----------------------------------------------------------------------------
    # no-remote-cache | Results in the action or test never being cached remotely (but it may
    #                 | be cached locally; it may also be executed remotely). Note: for the purposes
    #                 | of this tag, the disk-cache is considered a local cache, whereas the http
    #                 | and gRPC caches are considered remote. If a combined cache is specified
    #                 | (i.e. a cache with local and remote components), it's treated as a remote
    #                 | cache and disabled entirely unless --incompatible_remote_results_ignore_disk
    #                 | is set in which case the local components will be used.
    # ----------------+-----------------------------------------------------------------------------
    # no-remote-exec  | Results in the action or test never being executed remotely (but it may be
    #                 | cached remotely).
    # ----------------+-----------------------------------------------------------------------------
    # no-cache        | Results in the action or test never being cached (remotely or locally)
    # ----------------+-----------------------------------------------------------------------------
    # no-sandbox      | Results in the action or test never being sandboxed; it can still be cached
    #                 | or run remotely - use no-cache or no-remote to prevent either or both of
    #                 | those.
    # ----------------+-----------------------------------------------------------------------------
    # local           | Precludes the action or test from being remotely cached, remotely executed,
    #                 | or run inside the sandbox. For genrules and tests, marking the rule with the
    #                 | local = True attribute has the same effect.
    # ----------------+-----------------------------------------------------------------------------
    # See https://bazel.google.cn/reference/be/common-definitions?hl=en&authuser=0#common-attributes
    #
    # Copying file & directories is entirely IO-bound and there is no point doing this work
    # remotely.
    #
    # Also, remote-execution does not allow source directory inputs, see
    # https://github.com/bazelbuild/bazel/commit/c64421bc35214f0414e4f4226cc953e8c55fa0d2 So we must
    # not attempt to execute remotely in that case.
    #
    # There is also no point pulling the output file or directory from the remote cache since the
    # bytes to copy are already available locally. Conversely, no point in writing to the cache if
    # no one has any reason to check it for this action.
    #
    # Read and writing to disk cache is disabled as well primarily to reduce disk usage on the local
    # machine. A disk cache hit of a directory copy could be slghtly faster than a copy since the
    # disk cache stores the directory artifact as a single entry, but the slight performance bump
    # comes at the cost of heavy disk cache usage, which is an unmanaged directory that grow beyond
    # the bounds of the physical disk.
    # TODO: run benchmarks to measure the impact on copy_directory
    #
    # Sandboxing for this action is wasteful as well since there is a 1:1 mapping of input
    # file/directory to output file/directory and no room for non-hermetic inputs to sneak in to the
    # input.
    "no-remote": "1",
    "no-remote-cache": "1",
    "no-remote-exec": "1",
    "no-cache": "1",
    "no-sandbox": "1",
    "local": "1",
}
