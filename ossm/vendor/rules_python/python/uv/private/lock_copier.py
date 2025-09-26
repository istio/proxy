import sys
from difflib import unified_diff
from os import environ
from pathlib import Path

_LINE = "=" * 80


def main():
    src = "{{src}}"
    dst = "{{dst}}"

    src = Path(src)
    if not src.exists():
        raise AssertionError(f"The {src} file does not exist")

    if "TEST_SRCDIR" in environ:
        # Running as a bazel test
        dst = Path(dst)
        a = dst.read_text() if dst.exists() else "\n"
        b = src.read_text()

        diff = unified_diff(
            a.splitlines(),
            b.splitlines(),
            str(dst),
            str(src),
            lineterm="",
        )
        diff = "\n".join(list(diff))
        if not diff:
            print(
                f"""\
{_LINE}
The in source file copy is up-to-date.
{_LINE}
"""
            )
            return 0

        print(diff)
        print(
            f"""\
{_LINE}
The in source file copy is out of date, please run:

    bazel run {{update_target}}
{_LINE}
"""
        )
        return 1

    if "BUILD_WORKSPACE_DIRECTORY" not in environ:
        raise RuntimeError(
            "This must be either run as `bazel test` via a `native_test` or similar or via `bazel run`"
        )

    print(f"cp <bazel-sandbox>/{src} <workspace>/{dst}")
    build_workspace = Path(environ["BUILD_WORKSPACE_DIRECTORY"])

    dst_real_path = build_workspace / dst
    dst_real_path.parent.mkdir(parents=True, exist_ok=True)
    dst_real_path.write_text(src.read_text())
    print(f"OK: updated {dst_real_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
