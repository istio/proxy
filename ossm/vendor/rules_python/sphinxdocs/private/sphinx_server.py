import contextlib
import errno
import os
import sys
import time
from http import server

from python.runfiles import Runfiles


def main(argv):
    r = Runfiles.Create()
    serve_directory = r.Rlocation(argv[1])
    if not serve_directory:
        print(f"Error: could not find runfile for '{argv[1]}'", file=sys.stderr)
        return 1

    class DirectoryHandler(server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(directory=serve_directory, *args, **kwargs)

    address = ("0.0.0.0", 8000)
    # with server.ThreadingHTTPServer(address, DirectoryHandler) as (ip, port, httpd):
    with _start_server(DirectoryHandler, "0.0.0.0", 8000) as (ip, port, httpd):

        def _print_server_info():
            print(f"Serving...")
            print(f"  Address: http://{ip}:{port}")
            print(f"  Serving directory: {serve_directory}")
            print(f"      url: file://{serve_directory}")
            print(f"  Server CWD: {os.getcwd()}")
            print()
            print("*** You do not need to restart this server to see changes ***")
            print("*** CTRL+C once to reprint this info ***")
            print("*** CTRL+C twice to exit ***")
            print()

        while True:
            _print_server_info()
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                _print_server_info()
                print(
                    "*** KeyboardInterrupt received: CTRL+C again to terminate server ***"
                )
                try:
                    time.sleep(1)
                    print("Restarting serving ...")
                except KeyboardInterrupt:
                    break
    return 0


@contextlib.contextmanager
def _start_server(handler, ip, start_port):
    for port in range(start_port, start_port + 10):
        try:
            with server.ThreadingHTTPServer((ip, port), handler) as httpd:
                yield ip, port, httpd
                return
        except OSError as e:
            if e.errno == errno.EADDRINUSE:
                pass
            else:
                raise
    raise ValueError("Unable to find an available port")


if __name__ == "__main__":
    sys.exit(main(sys.argv))
