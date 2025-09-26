#!/usr/bin/env python3

# Copyright 2021 The Bazel Authors. All rights reserved.
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

# This template is completed by `pkg_install` to create installation scripts,
# and will not function on its own.  See pkg/install.bzl for more details.

import argparse
import logging
import os
import pathlib
import shutil
import sys
import tempfile

from pkg.private import manifest

# Globals used for runfile path manipulation.
#
# These are necessary because runfiles are different when used as a part of
# `bazel build` and `bazel run`. # See also
# https://docs.bazel.build/versions/4.1.0/skylark/rules.html#tools-with-runfiles

# Bazel's documentation claims these are set when calling `bazel run`, but not other
# modes, like in `build` or `test`.  We'll see.
CALLED_FROM_BAZEL_RUN = bool(os.getenv("BUILD_WORKSPACE_DIRECTORY") and
                             os.getenv("BUILD_WORKING_DIRECTORY"))

WORKSPACE_NAME = "{WORKSPACE_NAME}"
# This seems to be set when running in `bazel build` or `bazel test`
# TODO(#382): This may not be the case in Windows.
RUNFILE_PREFIX = os.path.join(os.getenv("RUNFILES_DIR"), WORKSPACE_NAME) if os.getenv("RUNFILES_DIR") else None


# This is named "NativeInstaller" because it makes use of "native" python
# functionality for installing files that should be cross-platform.
#
# A variant on this might be an installer at least partially based on coreutils.
# Most notably, some filesystems on Linux (and maybe others) support
# copy-on-write functionality that are known to tools like cp(1) and install(1)
# but may not be in the available python runtime.
#
# See also https://bugs.python.org/issue37157.
class NativeInstaller(object):
    def __init__(self, default_user=None, default_group=None, destdir=None,
                 wipe_destdir=False):
        self.default_user = default_user
        self.default_group = default_group
        self.destdir = destdir
        self.wipe_destdir = wipe_destdir
        self.entries = []

    # Logger helper method, may not be necessary or desired
    def _subst_destdir(path, self):
        return path.replace(self.destdir, "$DESTDIR")

    def _chown_chmod(self, dest, mode, user, group):
        if mode:
            logging.debug("CHMOD %s %s", mode, dest)
            os.chmod(dest, int(mode, 8))
        if user or group:
            # Ownership can only be changed by sufficiently
            # privileged users.
            # TODO(nacl): This does not support windows
            if hasattr(os, "getuid") and os.getuid() == 0:
                logging.debug("CHOWN %s:%s %s", user, group, dest)
                shutil.chown(dest, user, group)

    def _do_file_copy(self, src, dest):
        logging.debug("COPY %s <- %s", dest, src)
        # Copy to a temporary file and then move it to the destination.
        # This ensures code-signed executables on certain platforms
        # behave correctly.
        # See: https://developer.apple.com/documentation/security/updating-mac-software
        # Use `dir` to ensure the temporary file is created on the same file system as the destination,
        # to avoid cross-filesystem replace which is an error on some platforms.
        with tempfile.NamedTemporaryFile(delete=False, dir=os.path.dirname(dest)) as tmp_file:
            try:
                shutil.copyfile(src, tmp_file.name)
                os.replace(tmp_file.name, dest)
            except:
                pathlib.Path(tmp_file.name).unlink(missing_ok=True)
                raise

    def _do_mkdir(self, dirname, mode):
        logging.debug("MKDIR %s %s", mode, dirname)
        os.makedirs(dirname, int(mode, 8), exist_ok=True)

    def _do_symlink(self, target, link_name, mode, user, group):
        raise NotImplementedError("symlinking not yet supported")

    def _maybe_make_unowned_dir(self, path):
        logging.debug("MKDIR (unowned) %s", path)
        # TODO(nacl): consider default permissions here
        # TODO(nacl): consider default ownership here
        os.makedirs(path, 0o755, exist_ok=True)

    def _install_file(self, entry):
        self._maybe_make_unowned_dir(os.path.dirname(entry.dest))
        self._do_file_copy(entry.src, entry.dest)
        self._chown_chmod(entry.dest, entry.mode, entry.user, entry.group)

    def _install_directory(self, entry):
        self._maybe_make_unowned_dir(os.path.dirname(entry.dest))
        self._do_mkdir(entry.dest, entry.mode)
        self._chown_chmod(entry.dest, entry.mode, entry.user, entry.group)

    def _install_treeartifact_file(self, entry, src, dst):
        self._do_file_copy(src, dst)
        self._chown_chmod(dst, entry.mode, entry.user, entry.group)

    def _install_treeartifact(self, entry):
        logging.debug("COPYTREE %s <- %s/**", entry.dest, entry.src)
        shutil.copytree(
            src=entry.src,
            dst=entry.dest,
            copy_function=lambda s, d:
                self._install_treeartifact_file(entry, s, d),
            dirs_exist_ok=True,
            # Bazel gives us a directory of symlinks, so we dereference it.
            # TODO: Handle symlinks within the TreeArtifact. This is not yet
            # tested for other rules (e.g.
            # https://github.com/bazelbuild/rules_pkg/issues/750)
            symlinks=False,
            ignore_dangling_symlinks=True,
        )

        # Set mode/user/group for intermediate directories.
        # Bazel has no API to specify modes for this, so the least surprising
        # thing we can do is make it the canonical rwxr-xr-x
        intermediate_dir_mode = "755"
        for root, dirs, _ in os.walk(entry.src, topdown=False):
            relative_installdir = os.path.join(entry.dest,
                                               os.path.relpath(root, entry.src))
            for d in dirs:
                self._chown_chmod(os.path.join(relative_installdir, d),
                                  intermediate_dir_mode,
                                  entry.user, entry.group)

        # For top-level directory, use entry.mode +r +x if specified, otherwise
        # use least-surprising canonical rwxr-xr-x
        top_dir_mode = entry.mode
        if top_dir_mode:
            top_dir_mode = int(top_dir_mode, 8)
            top_dir_mode |= 0o555
            top_dir_mode = oct(top_dir_mode).removeprefix("0o")
        else:
            top_dir_mode = "755"
        self._chown_chmod(entry.dest, top_dir_mode, entry.user, entry.group)

    def _install_symlink(self, entry):
        raise NotImplementedError("symlinking not yet supported")
        logging.debug("SYMLINK %s <- %s", entry.dest, entry.link_to)
        logging.debug("CHMOD %s %s", entry.dest, entry.mode)
        logging.debug("CHOWN %s.%s %s", entry.dest, entry.user, entry.group)

    def include_manifest_path(self, path):
        with open(path, 'r') as fh:
            self.include_manifest(fh)

    def include_manifest(self, manifest_fh):
        manifest_entries = manifest.read_entries_from(manifest_fh)

        for entry in manifest_entries:
            # Swap out the source with the actual "runfile" location if we're
            # called as a part of the build rather than "bazel run"
            if not CALLED_FROM_BAZEL_RUN and entry.src is not None:
                entry.src = os.path.join(RUNFILE_PREFIX, entry.src)
            # Prepend the destdir path to all installation paths, if one is
            # specified.
            if self.destdir is not None:
                entry.dest = os.path.join(self.destdir, entry.dest)
            self.entries.append(entry)

    def do_the_thing(self):
        logging.info("Installing to %s", self.destdir)
        if self.wipe_destdir:
            logging.debug("RM %s", self.destdir)
            shutil.rmtree(self.destdir, ignore_errors=True)
        for entry in self.entries:
            if entry.type == manifest.ENTRY_IS_FILE:
                self._install_file(entry)
            elif entry.type == manifest.ENTRY_IS_LINK:
                self._install_symlink(entry)
            elif entry.type == manifest.ENTRY_IS_DIR:
                self._install_directory(entry)
            elif entry.type == manifest.ENTRY_IS_TREE:
                self._install_treeartifact(entry)
            else:
                raise ValueError("Unrecognized entry type '{}'".format(entry.type))


def _default_destdir():
    # If --destdir is not specified, use these values, in this order
    # Use env var if specified and non-empty
    env = os.getenv("DESTDIR")
    if env:
        return env

    # Checks if DEFAULT_DESTDIR is an empty string
    target_attr = "{DEFAULT_DESTDIR}"
    if target_attr:
        return target_attr

    return None


def _resolve_destdir(path_s):
    if not path_s:
        raise argparse.ArgumentTypeError("destdir is not set!")
    path = pathlib.Path(path_s)
    if path.is_absolute():
        return path_s
    build_workspace_directory = os.getenv("BUILD_WORKSPACE_DIRECTORY")
    if not build_workspace_directory:
        raise argparse.ArgumentTypeError(f"BUILD_WORKSPACE_DIRECTORY is not set"
                                         f" and destdir {path} is relative. "
                                         f"Unable to infer an absolute path.")
    ret = str(pathlib.Path(build_workspace_directory) / path)
    return ret


def main(args):
    parser = argparse.ArgumentParser(
        prog="bazel run -- {TARGET_LABEL}",
        description='Installer for bazel target {TARGET_LABEL}',
        fromfile_prefix_chars='@',
    )
    parser.add_argument('-v', '--verbose', action='count', default=0,
                        help="Be verbose.  Specify multiple times to increase verbosity further")
    parser.add_argument('-q', '--quiet', action='store_true', default=False,
                        help="Be silent, except for errors")
    # TODO(nacl): consider supporting DESTDIR=/whatever syntax, like "make
    # install".
    default_destdir = _default_destdir()
    default_destdir_text = f" or {default_destdir}" if default_destdir else ""
    parser.add_argument('--destdir', action='store', default=default_destdir,
                        required=default_destdir is None,
                        type=_resolve_destdir,
                        help=f"Installation root directory (defaults to DESTDIR"
                             f" environment variable{default_destdir_text}). "
                             f"Relative paths are interpreted against "
                             f"BUILD_WORKSPACE_DIRECTORY "
                             f"({os.getenv('BUILD_WORKSPACE_DIRECTORY')})")
    parser.add_argument("--wipe_destdir", action="store_true", default=False,
                        help="Delete destdir tree (including destdir itself) "
                             "before installing")

    args = parser.parse_args()

    loudness = args.verbose - args.quiet

    if args.quiet:
        level = logging.ERROR
    elif loudness == 0:
        level = logging.INFO
    else:  # loudness >= 1
        level = logging.DEBUG
    logging.basicConfig(
        level=level, format="%(levelname)s: %(message)s"
    )

    installer = NativeInstaller(
        destdir=args.destdir,
        wipe_destdir=args.wipe_destdir,
    )

    if not CALLED_FROM_BAZEL_RUN and RUNFILE_PREFIX is None:
        logging.critical("RUNFILES_DIR must be set in your environment when this is run as a bazel build tool.")
        logging.critical("This is most likely an issue on Windows.  See https://github.com/bazelbuild/rules_pkg/issues/387.")
        return 1

    for f in ["{MANIFEST_INCLUSION}"]:
        if CALLED_FROM_BAZEL_RUN:
            installer.include_manifest_path(f)
        else:
            installer.include_manifest_path(os.path.join(RUNFILE_PREFIX, f))

    installer.do_the_thing()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
