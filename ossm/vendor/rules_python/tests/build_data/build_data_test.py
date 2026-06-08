import unittest

from python.runfiles import runfiles


class BuildDataTest(unittest.TestCase):

    def test_target_build_data(self):
        import bazel_binary_info

        self.assertIn("build_data.txt", bazel_binary_info.BUILD_DATA_FILE)

        build_data = bazel_binary_info.get_build_data()
        self.assertIn("TARGET ", build_data)
        self.assertIn("BUILD_HOST ", build_data)
        self.assertIn("BUILD_USER ", build_data)
        self.assertIn("BUILD_TIMESTAMP ", build_data)
        self.assertIn("FORMATTED_DATE ", build_data)
        self.assertIn("CONFIG_MODE TARGET", build_data)
        self.assertIn("STAMPED TRUE", build_data)

    def test_tool_build_data(self):
        rf = runfiles.Create()
        path = rf.Rlocation("rules_python/tests/build_data/tool_build_data.txt")
        with open(path) as fp:
            build_data = fp.read()

        self.assertIn("STAMPED FALSE", build_data)
        self.assertIn("CONFIG_MODE EXEC", build_data)


unittest.main()
