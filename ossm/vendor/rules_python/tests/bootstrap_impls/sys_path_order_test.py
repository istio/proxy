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

import os.path
import re
import sys
import unittest


class SysPathOrderTest(unittest.TestCase):
    def test_sys_path_order(self):
        last_stdlib = None
        first_user = None
        first_runtime_site = None

        # Classify paths into the three different types we care about: stdlib,
        # user dependency, or the runtime's site-package's directory.
        #
        # Because they often share common prefixes with one another, and vary
        # subtly between platforms, we do this in two passes: first categorize,
        # then pick out the indexes. This is just so debugging is easier and
        # error messages are more informative.
        categorized_paths = []
        for i, value in enumerate(sys.path):
            # The runtime's root repo may be added to sys.path, but it
            # counts as a user directory, not stdlib directory.
            if value in (sys.prefix, sys.base_prefix):
                category = "user"
            elif value.startswith(sys.base_prefix):
                # The runtime's site-package directory might be called
                # dist-packages when using Debian's system python.
                if os.path.basename(value).endswith("-packages"):
                    category = "runtime-site"
                else:
                    category = "stdlib"
            else:
                category = "user"

            categorized_paths.append((category, value))

        for i, (category, _) in enumerate(categorized_paths):
            if category == "stdlib":
                last_stdlib = i
            elif category == "runtime-site":
                if first_runtime_site is None:
                    first_runtime_site = i
            elif category == "user":
                if first_user is None:
                    first_user = i

        sys_path_str = "\n".join(
            f"{i}: ({category}) {value}"
            for i, (category, value) in enumerate(categorized_paths)
        )
        if None in (last_stdlib, first_user, first_runtime_site):
            self.fail(
                "Failed to find position for one of:\n"
                + f"{last_stdlib=} {first_user=} {first_runtime_site=}\n"
                + f"for sys.prefix={sys.prefix}\n"
                + f"for sys.exec_prefix={sys.exec_prefix}\n"
                + f"for sys.base_prefix={sys.base_prefix}\n"
                + f"for sys.path:\n{sys_path_str}"
            )

        if os.environ["BOOTSTRAP"] == "script":
            self.assertTrue(
                last_stdlib < first_user < first_runtime_site,
                "Expected overall order to be (stdlib, user imports, runtime site) "
                + f"with {last_stdlib=} < {first_user=} < {first_runtime_site=}\n"
                + f"for sys.prefix={sys.prefix}\n"
                + f"for sys.exec_prefix={sys.exec_prefix}\n"
                + f"for sys.base_prefix={sys.base_prefix}\n"
                + f"for sys.path:\n{sys_path_str}",
            )
        else:
            self.assertTrue(
                first_user < last_stdlib < first_runtime_site,
                f"Expected {first_user=} < {last_stdlib=} < {first_runtime_site=}\n"
                + f"for sys.prefix={sys.prefix}\n"
                + f"for sys.exec_prefix={sys.exec_prefix}\n"
                + f"for sys.base_prefix={sys.base_prefix}\n"
                + f"for sys.path:\n{sys_path_str}",
            )


if __name__ == "__main__":
    unittest.main()
