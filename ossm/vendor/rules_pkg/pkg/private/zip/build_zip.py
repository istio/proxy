# Copyright 2015 The Bazel Authors. All rights reserved.
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
"""This tool builds zip files from a list of inputs."""

import argparse
import datetime
import logging
import os
import sys
import zipfile

from pkg.private import build_info
from pkg.private import manifest

ZIP_EPOCH = 315532800

# Unix dir bit and Windows dir bit. Magic from zip spec
UNIX_FILE_BIT =    0o100000
UNIX_SYMLINK_BIT = 0o120000
UNIX_DIR_BIT  =    0o040000
MSDOS_DIR_BIT = 0x10

def _create_argument_parser():
  """Creates the command line arg parser."""
  parser = argparse.ArgumentParser(description='create a zip file',
                                   fromfile_prefix_chars='@')
  parser.add_argument('-o', '--output', type=str,
                      help='The output zip file path.')
  parser.add_argument(
      '-d', '--directory', type=str, default='/',
      help='An absolute path to use as a prefix for all files in the zip.')
  parser.add_argument(
      '-t', '--timestamp', type=int, default=ZIP_EPOCH,
      help='The unix time to use for files added into the zip. values prior to'
           ' Jan 1, 1980 are ignored.')
  parser.add_argument('--stamp_from', default='',
                      help='File to find BUILD_STAMP in')
  parser.add_argument(
      '-m', '--mode',
      help='The file system mode to use for files added into the zip.')
  parser.add_argument(
      '-c', '--compression_type',
      help='The compression type to use')
  parser.add_argument(
      '-l', '--compression_level',
      help='The compression level to use')
  parser.add_argument('--manifest',
                      help='manifest of contents to add to the layer.',
                      required=True)
  parser.add_argument(
      'files', type=str, nargs='*',
      help='Files to be added to the zip, in the form of {srcpath}={dstpath}.')
  return parser


def _combine_paths(left, right):
  result = left.rstrip('/') + '/' + right.lstrip('/')

  # important: remove leading /'s: the zip format spec says paths should never
  # have a leading slash, but Python will happily do this. The built-in zip
  # tool in Windows will complain that such a zip file is invalid.
  return result.lstrip('/')


def parse_date(ts):
  ts = datetime.datetime.fromtimestamp(ts, tz=datetime.timezone.utc)
  return (ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second)


class ZipWriter(object):

  def __init__(self, output_path: str, time_stamp: int, default_mode: int, compression_type: str, compression_level: int):
    """Create a writer.

    You must close() after use or use in a 'with' statement.

    Args:
      output_path: path to write to
      time_stamp: time stamp to add to files
      default_mode: file mode to use if not specified in the entry.
    """
    self.output_path = output_path
    self.time_stamp = time_stamp
    self.default_mode = default_mode
    compressions = {
      "deflated": zipfile.ZIP_DEFLATED,
      "lzma": zipfile.ZIP_LZMA,
      "bzip2": zipfile.ZIP_BZIP2,
      "stored": zipfile.ZIP_STORED
    }
    self.compression_type = compressions[compression_type]
    self.compression_level = compression_level
    self.zip_file = zipfile.ZipFile(self.output_path, mode='w', compression=self.compression_type)

  def __enter__(self):
    return self

  def __exit__(self, t, v, traceback):
    self.close()

  def close(self):
    self.zip_file.close()
    self.zip_file = None

  def writestr(self, entry_info, content: str, compresslevel: int):
    if sys.version_info >= (3, 7):
      self.zip_file.writestr(entry_info, content, compresslevel=compresslevel)
    else:
      # Python 3.6 and lower don't support compresslevel
      self.zip_file.writestr(entry_info, content)
      if compresslevel != 6:
        logging.warn("Custom compresslevel is not supported with python < 3.7")

  def make_zipinfo(self, path: str, mode: str):
    """Create a Zipinfo.

    Args:
      path: file path
      mode: file mode
    """
    entry_info = zipfile.ZipInfo(filename=path, date_time=self.time_stamp)
    # See http://www.pkware.com/documents/casestudies/APPNOTE.TXT
    # denotes UTF-8 encoded file name.
    entry_info.flag_bits |= 0x800

    # See: https://trac.edgewall.org/attachment/ticket/8919/ZipDownload.patch
    # external_attr is 4 bytes in size. The high order two bytes represent UNIX
    # permission and file type bits, while the low order two contain MS-DOS FAT
    # file attributes.
    if mode:
      f_mode = int(mode, 8)
    else:
      f_mode = self.default_mode
    entry_info.external_attr = f_mode << 16
    return entry_info

  def add_manifest_entry(self, entry):
    """Add an entry to the zip file.

    Args:
      zip_file: ZipFile to write to
      entry: manifest entry
    """

    entry_type = entry.type
    dest = entry.dest
    src = entry.src
    mode = entry.mode
    user = entry.user
    group = entry.group

    # Use the pkg_tar mode/owner remapping as a fallback
    dst_path = dest.strip('/')
    if entry_type == manifest.ENTRY_IS_DIR and not dst_path.endswith('/'):
      dst_path += '/'
    entry_info = self.make_zipinfo(path=dst_path, mode=mode)

    if entry_type == manifest.ENTRY_IS_FILE:
      entry_info.compress_type = self.compression_type
      # Using utf-8 for the file names is for python <3.7 compatibility.
      entry_info.external_attr |= UNIX_FILE_BIT << 16
      with open(src.encode('utf-8'), 'rb') as src_content:
        self.writestr(entry_info, src_content.read(), compresslevel=self.compression_level)
    elif entry_type == manifest.ENTRY_IS_DIR:
      entry_info.compress_type = zipfile.ZIP_STORED
      # Set directory bits
      entry_info.external_attr |= (UNIX_DIR_BIT << 16) | MSDOS_DIR_BIT
      self.zip_file.writestr(entry_info, '')
    elif entry_type == manifest.ENTRY_IS_LINK:
      entry_info.compress_type = zipfile.ZIP_STORED
      # Set directory bits
      entry_info.external_attr |= (UNIX_SYMLINK_BIT << 16)
      self.zip_file.writestr(entry_info, src.encode('utf-8'))
    elif entry_type == manifest.ENTRY_IS_TREE:
      self.add_tree(src, dst_path, mode)
    elif entry_type == manifest.ENTRY_IS_EMPTY_FILE:
      entry_info.compress_type = zipfile.ZIP_STORED
      self.zip_file.writestr(entry_info, '')
    else:
      raise Exception('Unknown type for manifest entry:', entry)

  def add_tree(self, tree_top: str, destpath: str, mode: int):
    """Add a tree artifact to the zip file.

    Args:
      tree_top: the top of the tree to add
      destpath: the path under which to place the files
      mode: if not None, file mode to apply to all files
    """

    # We expect /-style paths.
    tree_top = os.path.normpath(tree_top).replace(os.path.sep, '/')

    # Again, we expect /-style paths.
    dest = destpath.strip('/')  # redundant, dests should never have / here
    dest = os.path.normpath(dest).replace(os.path.sep, '/')
    # paths should not have a leading ./
    dest = '' if dest == '.' else dest + '/'

    to_write = {}
    for root, dirs, files in os.walk(tree_top):
      # While `tree_top` uses '/' as a path separator, results returned by
      # `os.walk` and `os.path.join` on Windows may not.
      root = os.path.normpath(root).replace(os.path.sep, '/')

      rel_path_from_top = root[len(tree_top):].lstrip('/')
      if rel_path_from_top:
        dest_dir = dest + rel_path_from_top + '/'
      else:
        dest_dir = dest
      to_write[dest_dir] = None
      for file in files:
        content_path = os.path.abspath(os.path.join(root, file))
        if os.name == "nt":
          # "To specify an extended-length path, use the `\\?\` prefix. For
          # example, `\\?\D:\very long path`."[1]
          #
          # [1]: https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
          to_write[dest_dir + file] = "\\\\?\\" + content_path
        else:
          to_write[dest_dir + file] = content_path

    for path in sorted(to_write.keys()):
      content_path = to_write[path]
      if content_path:
        # If mode is unspecified, derive the mode from the file's mode.
        if mode is None:
          f_mode = "0o755" if os.access(content_path, os.X_OK) else self.default_mode
        else:
          f_mode = mode
        entry_info = self.make_zipinfo(path=path, mode=f_mode)
        entry_info.compress_type = self.compression_type
        with open(content_path, 'rb') as src:
          self.writestr(entry_info, src.read(), compresslevel=self.compression_level)
      else:
        # Implicitly created directory
        dir_path = path
        if not dir_path.endswith('/'):
          dir_path += '/'
        entry_info = self.make_zipinfo(path=dir_path, mode="0o755")
        entry_info.compress_type = zipfile.ZIP_STORED
        # Set directory bits
        entry_info.external_attr |= (UNIX_DIR_BIT << 16) | MSDOS_DIR_BIT
        self.zip_file.writestr(entry_info, '')

def _load_manifest(prefix, manifest_path):
  manifest_map = {}

  for entry in manifest.read_entries_from_file(manifest_path):
    entry.dest = _combine_paths(prefix, entry.dest)
    manifest_map[entry.dest] = entry

  # We modify the dictionary as we're iterating over it, so we need to listify
  # the keys here.
  manifest_keys = list(manifest_map.keys())
  # Add all parent directories of entries that have not been added explicitly.
  for dest in manifest_keys:
      parent = dest
      # TODO: use pathlib instead of string manipulation?
      for _ in range(dest.count("/")):
        parent, _, _ = parent.rpartition("/")
        if parent and parent not in manifest_map:
            manifest_map[parent] = manifest.ManifestEntry(
              type = manifest.ENTRY_IS_DIR,
              dest = parent,
              src = "",
              mode = "0o755",
              user =  None,
              group = None,
              uid = None,
              gid = None,
              origin = "parent directory of {}".format(manifest_map[dest].origin),
            )

  return sorted(manifest_map.values(), key = lambda x: x.dest)

def main(args):
  unix_ts = max(ZIP_EPOCH, args.timestamp)
  if args.stamp_from:
    unix_ts = build_info.get_timestamp(args.stamp_from)
  ts = parse_date(unix_ts)
  default_mode = None
  if args.mode:
    default_mode = int(args.mode, 8)
  compression_level = int(args.compression_level)

  manifest = _load_manifest(args.directory, args.manifest)
  with ZipWriter(
      args.output, time_stamp=ts, default_mode=default_mode, compression_type=args.compression_type, compression_level=compression_level) as zip_out:
    for entry in manifest:
      zip_out.add_manifest_entry(entry)


if __name__ == '__main__':
  arg_parser = _create_argument_parser()
  main(arg_parser.parse_args())
