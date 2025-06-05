# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import unittest

from python.private.pypi.whl_installer import arguments, wheel


class ArgumentsTestCase(unittest.TestCase):
    def test_arguments(self) -> None:
        parser = arguments.parser()
        index_url = "--index_url=pypi.org/simple"
        extra_pip_args = [index_url]
        requirement = "foo==1.0.0 --hash=sha256:deadbeef"
        args_dict = vars(
            parser.parse_args(
                args=[
                    f'--requirement="{requirement}"',
                    f"--extra_pip_args={json.dumps({'arg': extra_pip_args})}",
                ]
            )
        )
        args_dict = arguments.deserialize_structured_args(args_dict)
        self.assertIn("requirement", args_dict)
        self.assertIn("extra_pip_args", args_dict)
        self.assertEqual(args_dict["pip_data_exclude"], [])
        self.assertEqual(args_dict["enable_implicit_namespace_pkgs"], False)
        self.assertEqual(args_dict["extra_pip_args"], extra_pip_args)

    def test_deserialize_structured_args(self) -> None:
        serialized_args = {
            "pip_data_exclude": json.dumps({"arg": ["**.foo"]}),
            "environment": json.dumps({"arg": {"PIP_DO_SOMETHING": "True"}}),
        }
        args = arguments.deserialize_structured_args(serialized_args)
        self.assertEqual(args["pip_data_exclude"], ["**.foo"])
        self.assertEqual(args["environment"], {"PIP_DO_SOMETHING": "True"})
        self.assertEqual(args["extra_pip_args"], [])

    def test_platform_aggregation(self) -> None:
        parser = arguments.parser()
        args = parser.parse_args(
            args=[
                "--platform=linux_*",
                "--platform=osx_*",
                "--platform=windows_*",
                "--requirement=foo",
            ]
        )
        self.assertEqual(set(wheel.Platform.all()), arguments.get_platforms(args))


if __name__ == "__main__":
    unittest.main()
