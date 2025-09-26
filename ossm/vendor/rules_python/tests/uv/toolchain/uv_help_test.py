#!/usr/bin/env python

import os
import unittest
from pathlib import Path

from python.runfiles import runfiles


class TestUV(unittest.TestCase):
    def test_uv_help(self):
        rfiles = runfiles.Create()
        assert rfiles is not None, "rfiles creation failed"

        data_rpath = os.environ["DATA"]
        uv_help_path = rfiles.Rlocation(data_rpath)
        assert (
            uv_help_path is not None
        ), f"the rlocation path was not found: {data_rpath}"

        uv_help = Path(uv_help_path).read_text()

        self.assertIn("Usage: uv [OPTIONS] <COMMAND>", uv_help)


if __name__ == "__main__":
    unittest.main()
