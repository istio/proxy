import os
import socket
import subprocess
import textwrap
import time
import unittest
from contextlib import closing
from pathlib import Path
from urllib.request import urlopen


def find_free_port():
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("", 0))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        return s.getsockname()[1]


class TestTwineUpload(unittest.TestCase):
    def setUp(self):
        self.maxDiff = 1000
        self.port = find_free_port()
        self.url = f"http://localhost:{self.port}"
        self.dir = Path(os.environ["TEST_TMPDIR"])

        self.log_file = self.dir / "pypiserver-log.txt"
        self.log_file.touch()
        _storage_dir = self.dir / "data"
        for d in [_storage_dir]:
            d.mkdir(exist_ok=True)

        print("Starting PyPI server...")
        self._server = subprocess.Popen(
            [
                str(Path(os.environ["SERVER_PATH"])),
                "run",
                "--verbose",
                "--log-file",
                str(self.log_file),
                "--host",
                "localhost",
                "--port",
                str(self.port),
                # Allow unauthenticated access
                "--authenticate",
                ".",
                "--passwords",
                ".",
                str(_storage_dir),
            ],
        )

        line = "Hit Ctrl-C to quit"
        interval = 0.1
        wait_seconds = 40
        for _ in range(int(wait_seconds / interval)):  # 40 second timeout
            current_logs = self.log_file.read_text()
            if line in current_logs:
                print(current_logs.strip())
                print("...")
                break

            time.sleep(0.1)
        else:
            raise RuntimeError(
                f"Could not get the server running fast enough, waited for {wait_seconds}s"
            )

    def tearDown(self):
        self._server.terminate()
        print(f"Stopped PyPI server, all logs:\n{self.log_file.read_text()}")

    def test_upload_and_query_simple_api(self):
        # Given
        script_path = Path(os.environ["PUBLISH_PATH"])
        whl = Path(os.environ["WHEEL_PATH"])

        # When I publish a whl to a package registry
        subprocess.check_output(
            [
                str(script_path),
                "--no-color",
                "upload",
                str(whl),
                "--verbose",
                "--non-interactive",
                "--disable-progress-bar",
            ],
            env={
                "TWINE_REPOSITORY_URL": self.url,
                "TWINE_USERNAME": "dummy",
                "TWINE_PASSWORD": "dummy",
            },
        )

        # Then I should be able to get its contents
        with urlopen(self.url + "/example-minimal-library/") as response:
            got_content = response.read().decode("utf-8")
            want_content = """
<!DOCTYPE html>
<html>
    <head>
        <title>Links for example-minimal-library</title>
    </head>
    <body>
        <h1>Links for example-minimal-library</h1>
             <a href="/packages/example_minimal_library-0.0.1-py3-none-any.whl#sha256=79a4e9c1838c0631d5d8fa49a26efd6e9a364f6b38d9597c0f6df112271a0e28">example_minimal_library-0.0.1-py3-none-any.whl</a><br>
    </body>
</html>"""
            self.assertEqual(
                textwrap.dedent(want_content).strip(),
                textwrap.dedent(got_content).strip(),
            )


if __name__ == "__main__":
    unittest.main()
