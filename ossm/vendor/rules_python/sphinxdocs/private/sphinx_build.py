import contextlib
import io
import json
import logging
import os
import shutil
import sys
import traceback
import typing

import sphinx.application
from sphinx.cmd.build import main

WorkRequest = object
WorkResponse = object


class SphinxMainError(Exception):
    def __init__(self, message, exit_code):
        super().__init__(message)
        self.exit_code = exit_code


logger = logging.getLogger("sphinxdocs_build")

_WORKER_SPHINX_EXT_MODULE_NAME = "bazel_worker_sphinx_ext"

# Config value name for getting the path to the request info file
_REQUEST_INFO_CONFIG_NAME = "bazel_worker_request_info_path"


class Worker:

    def __init__(
        self, instream: "typing.TextIO", outstream: "typing.TextIO", exec_root: str
    ):
        # NOTE: Sphinx performs its own logging re-configuration, so any
        # logging config we do isn't respected by Sphinx. Controlling where
        # stdout and stderr goes are the main mechanisms. Recall that
        # Bazel send worker stderr to the worker log file.
        # outputBase=$(bazel info output_base)
        # find $outputBase/bazel-workers/ -type f -printf '%T@ %p\n' | sort -n | tail -1 | awk '{print $2}'
        logging.basicConfig(level=logging.WARN)
        logger.info("Initializing worker")

        # The directory that paths are relative to.
        self._exec_root = exec_root
        # Where requests are read from.
        self._instream = instream
        # Where responses are written to.
        self._outstream = outstream

        # dict[str srcdir, dict[str path, str digest]]
        self._digests = {}

        # Internal output directories the worker gives to Sphinx that need
        # to be cleaned up upon exit.
        # set[str path]
        self._worker_outdirs = set()
        self._extension = BazelWorkerExtension()

        sys.modules[_WORKER_SPHINX_EXT_MODULE_NAME] = self._extension
        sphinx.application.builtin_extensions += (_WORKER_SPHINX_EXT_MODULE_NAME,)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        for worker_outdir in self._worker_outdirs:
            shutil.rmtree(worker_outdir, ignore_errors=True)

    def run(self) -> None:
        logger.info("Worker started")
        try:
            while True:
                request = None
                try:
                    request = self._get_next_request()
                    if request is None:
                        logger.info("Empty request: exiting")
                        break
                    response = self._process_request(request)
                    if response:
                        self._send_response(response)
                except SphinxMainError as e:
                    logger.error("Sphinx main returned failure: exit_code=%s request=%s",
                                 request, e.exit_code)
                    request_id = 0 if not request else request.get("requestId", 0)
                    self._send_response(
                        {
                            "exitCode": e.exit_code,
                            "output": str(e),
                            "requestId": request_id,
                        }
                    )
                except Exception:
                    logger.exception("Unhandled error: request=%s", request)
                    output = (
                        f"Unhandled error:\nRequest id: {request.get('id')}\n"
                        + traceback.format_exc()
                    )
                    request_id = 0 if not request else request.get("requestId", 0)
                    self._send_response(
                        {
                            "exitCode": 3,
                            "output": output,
                            "requestId": request_id,
                        }
                    )
        finally:
            logger.info("Worker shutting down")

    def _get_next_request(self) -> "object | None":
        line = self._instream.readline()
        if not line:
            return None
        return json.loads(line)

    def _send_response(self, response: "WorkResponse") -> None:
        self._outstream.write(json.dumps(response) + "\n")
        self._outstream.flush()

    def _prepare_sphinx(self, request):
        sphinx_args = request["arguments"]
        srcdir = sphinx_args[0]

        incoming_digests = {}
        current_digests = self._digests.setdefault(srcdir, {})
        changed_paths = []
        request_info = {"exec_root": self._exec_root, "inputs": request["inputs"]}
        for entry in request["inputs"]:
            path = entry["path"]
            digest = entry["digest"]
            # Make the path srcdir-relative so Sphinx understands it.
            path = path.removeprefix(srcdir + "/")
            incoming_digests[path] = digest

            if path not in current_digests:
                logger.info("path %s new", path)
                changed_paths.append(path)
            elif current_digests[path] != digest:
                logger.info("path %s changed", path)
                changed_paths.append(path)

        self._digests[srcdir] = incoming_digests
        self._extension.changed_paths = changed_paths
        request_info["changed_sources"] = changed_paths

        bazel_outdir = sphinx_args[1]
        worker_outdir = bazel_outdir + ".worker-out.d"
        self._worker_outdirs.add(worker_outdir)
        sphinx_args[1] = worker_outdir

        request_info_path = os.path.join(srcdir, "_bazel_worker_request_info.json")
        with open(request_info_path, "w") as fp:
            json.dump(request_info, fp)
        sphinx_args.append(f"--define={_REQUEST_INFO_CONFIG_NAME}={request_info_path}")

        return worker_outdir, bazel_outdir, sphinx_args

    @contextlib.contextmanager
    def _redirect_streams(self):
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            yield stdout, stderr

    def _process_request(self, request: "WorkRequest") -> "WorkResponse | None":
        logger.info("Request: %s", json.dumps(request, sort_keys=True, indent=2))
        if request.get("cancel"):
            return None

        worker_outdir, bazel_outdir, sphinx_args = self._prepare_sphinx(request)

        # Prevent anything from going to stdout because it breaks the worker
        # protocol. We have limited control over where Sphinx sends output.
        with self._redirect_streams() as (stdout, stderr):
            logger.info("main args: %s", sphinx_args)
            exit_code = main(sphinx_args)
            # Running Sphinx multiple times in a process can give spurious
            # errors. An invocation after an error seems to work, though.
            if exit_code == 2:
                logger.warning("Sphinx main() returned exit_code=2, retrying...")
                # Reset streams to capture output of the retry cleanly
                stdout.seek(0)
                stdout.truncate(0)
                stderr.seek(0)
                stderr.truncate(0)
                exit_code = main(sphinx_args)

        if exit_code:
            stdout_output = stdout.getvalue().strip()
            stderr_output = stderr.getvalue().strip()
            if stdout_output:
                stdout_output = (
                    "========== STDOUT START ==========\n"
                    + stdout_output
                    + "\n"
                    + "========== STDOUT END ==========\n"
                )
            else:
                stdout_output = "========== STDOUT EMPTY ==========\n"
            if stderr_output:
                stderr_output = (
                    "========== STDERR START ==========\n"
                    + stderr_output
                    + "\n"
                    + "========== STDERR END ==========\n"
                )
            else:
                stderr_output = "========== STDERR EMPTY ==========\n"

            message = (
                "Sphinx main() returned failure: "
                + f"  exit code: {exit_code}\n"
                + stdout_output
                + stderr_output
            )
            raise SphinxMainError(message, exit_code)


        # Copying is unfortunately necessary because Bazel doesn't know to
        # implicily bring along what the symlinks point to.
        shutil.copytree(worker_outdir, bazel_outdir, dirs_exist_ok=True)

        response = {
            "requestId": request.get("requestId", 0),
            "output": stdout.getvalue(),
            "exitCode": 0,
        }
        return response


class BazelWorkerExtension:
    """A Sphinx extension implemented as a class acting like a module."""

    def __init__(self):
        # Make it look like a Module object
        self.__name__ = _WORKER_SPHINX_EXT_MODULE_NAME
        # set[str] of src-dir relative path names
        self.changed_paths = set()

    def setup(self, app):
        app.add_config_value(_REQUEST_INFO_CONFIG_NAME, "", "")
        app.connect("env-get-outdated", self._handle_env_get_outdated)
        return {"parallel_read_safe": True, "parallel_write_safe": True}

    def _handle_env_get_outdated(self, app, env, added, changed, removed):
        changed = {
            # NOTE: path2doc returns None if it's not a doc path
            env.path2doc(p)
            for p in self.changed_paths
        }

        logger.info("changed docs: %s", changed)
        return changed


def _worker_main(stdin, stdout, exec_root):
    with Worker(stdin, stdout, exec_root) as worker:
        return worker.run()


def _non_worker_main():
    args = []
    for arg in sys.argv:
        if arg.startswith("@"):
            with open(arg.removeprefix("@")) as fp:
                lines = [line.strip() for line in fp if line.strip()]
            args.extend(lines)
        else:
            args.append(arg)
    sys.argv[:] = args
    return main()


if __name__ == "__main__":
    if "--persistent_worker" in sys.argv:
        sys.exit(_worker_main(sys.stdin, sys.stdout, os.getcwd()))
    else:
        sys.exit(_non_worker_main())
