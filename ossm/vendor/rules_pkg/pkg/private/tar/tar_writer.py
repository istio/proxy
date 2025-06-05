# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Tar writing helper."""

import gzip
import io
import os
import subprocess
import tarfile

try:
  import lzma  # pylint: disable=g-import-not-at-top, unused-import
  HAS_LZMA = True
except ImportError:
  HAS_LZMA = False

# This is slightly a lie. We do support xz fallback through the xz tool, but
# that is fragile. Users should stick to the expectations provided here.
COMPRESSIONS = ('', 'gz', 'bz2', 'xz') if HAS_LZMA else ('', 'gz', 'bz2')

# Use a deterministic mtime that doesn't confuse other programs.
# See: https://github.com/bazelbuild/bazel/issues/1299
PORTABLE_MTIME = 946684800  # 2000-01-01 00:00:00.000 UTC

_DEBUG_VERBOSITY = 0


class TarFileWriter(object):
  """A wrapper to write tar files."""

  class Error(Exception):
    pass

  def __init__(self,
               name,
               compression='',
               compressor='',
               create_parents=False,
               allow_dups_from_deps=True,
               default_mtime=None,
               preserve_tar_mtimes=True,
               compression_level=-1):
    """TarFileWriter wraps tarfile.open().

    Args:
      name: the tar file name.
      compression: compression type: bzip2, bz2, gz, tgz, xz, lzma.
      compressor: custom command to do the compression.
      default_mtime: default mtime to use for elements in the archive.
          May be an integer or the value 'portable' to use the date
          2000-01-01, which is compatible with non *nix OSes'.
      preserve_tar_mtimes: if true, keep file mtimes from input tar file.
    """
    self.preserve_mtime = preserve_tar_mtimes
    if default_mtime is None:
      self.default_mtime = 0
    elif default_mtime == 'portable':
      self.default_mtime = PORTABLE_MTIME
    else:
      self.default_mtime = int(default_mtime)

    self.fileobj = None
    self.compressor_cmd = (compressor or '').strip()
    if self.compressor_cmd:
      # Some custom command has been specified: no need for further
      # configuration, we're just going to use it.
      pass
    # Support xz compression through xz... until we can use Py3
    elif compression in ['xz', 'lzma']:
      if HAS_LZMA:
        mode = 'w:xz'
      else:
        self.compressor_cmd = 'xz -F {} -'.format(compression)
    elif compression in ['bzip2', 'bz2']:
      mode = 'w:bz2'
    else:
      mode = 'w:'
      if compression in ['tgz', 'gz']:
        compression_level = min(compression_level, 9) if compression_level >= 0 else 6
        # The Tarfile class doesn't allow us to specify gzip's mtime attribute.
        # Instead, we manually reimplement gzopen from tarfile.py and set mtime.
        self.fileobj = gzip.GzipFile(
            filename=name, mode='w', compresslevel=compression_level, mtime=self.default_mtime)
    self.compressor_proc = None
    if self.compressor_cmd:
      mode = 'w|'
      self.compressor_proc = subprocess.Popen(self.compressor_cmd.split(),
                                              stdin=subprocess.PIPE,
                                              stdout=open(name, 'wb'))
      self.fileobj = self.compressor_proc.stdin
    self.name = name

    self.tar = tarfile.open(name=name, mode=mode, fileobj=self.fileobj,
                            format=tarfile.GNU_FORMAT)
    self.members = set()
    self.directories = set()
    # Preseed the added directory list with things we should not add. If we
    # some day need to allow '.' or '/' as an explicit member of the archive,
    # we can adjust that here based on the setting of root_directory.
    self.directories.add('/')
    self.directories.add('./')
    self.create_parents = create_parents
    self.allow_dups_from_deps = allow_dups_from_deps

  def __enter__(self):
    return self

  def __exit__(self, t, v, traceback):
    self.close()

  def _have_added(self, path):
    """Have we added this file before."""
    return (path in self.members) or (path in self.directories)

  def _addfile(self, info, fileobj=None):
    """Add a file in the tar file if there is no conflict."""
    if info.type == tarfile.DIRTYPE:
      # Enforce the ending / for directories so we correctly deduplicate.
      if not info.name.endswith('/'):
        info.name += '/'
    if not self.allow_dups_from_deps and self._have_added(info.name):
      # Directories with different contents should get merged without warnings.
      # If they have overlapping content, the warning will be on their duplicate *files* instead
      if info.type != tarfile.DIRTYPE:
        print('Duplicate file in archive: %s, '
              'picking first occurrence' % info.name)
      return

    self.tar.addfile(info, fileobj)
    self.members.add(info.name)
    if info.type == tarfile.DIRTYPE:
      self.directories.add(info.name)

  def add_directory_path(self,
                         path,
                         uid=0,
                         gid=0,
                         uname='',
                         gname='',
                         mtime=None,
                         mode=0o755):
    """Add a directory to the current tar.

    Args:
      path: the ('/' delimited) path of the file to add.
      uid: owner user identifier.
      gid: owner group identifier.
      uname: owner user names.
      gname: owner group names.
      mtime: modification time to put in the archive.
      mode: unix permission mode of the file, default: 0755.
    """
    assert path[-1] == '/'
    if not path:
      return
    if _DEBUG_VERBOSITY > 1:
      print('DEBUG: adding directory', path)
    tarinfo = tarfile.TarInfo(path)
    tarinfo.type = tarfile.DIRTYPE
    tarinfo.mtime = mtime
    tarinfo.mode = mode
    tarinfo.uid = uid
    tarinfo.gid = gid
    tarinfo.uname = uname
    tarinfo.gname = gname
    self._addfile(tarinfo)

  def conditionally_add_parents(self, path, uid=0, gid=0, uname='', gname='', mtime=0, mode=0o755):
    dirs = path.split('/')
    parent_path = ''
    for next_level in dirs[0:-1]:
      parent_path = parent_path + next_level + '/'

      if self.create_parents and not self._have_added(parent_path):
        self.add_directory_path(
          parent_path,
          uid=uid,
          gid=gid,
          uname=uname,
          gname=gname,
          mtime=mtime,
          mode=0o755)

  def add_file(self,
               name,
               kind=tarfile.REGTYPE,
               content=None,
               link=None,
               file_content=None,
               uid=0,
               gid=0,
               uname='',
               gname='',
               mtime=None,
               mode=None):
    """Add a file to the current tar.

    Args:
      name: the ('/' delimited) path of the file to add.
      kind: the type of the file to add, see tarfile.*TYPE.
      content: the content to put in the file.
      link: if the file is a link, the destination of the link.
      file_content: file to read the content from. Provide either this
          one or `content` to specifies a content for the file.
      uid: owner user identifier.
      gid: owner group identifier.
      uname: owner user names.
      gname: owner group names.
      mtime: modification time to put in the archive.
      mode: unix permission mode of the file, default 0644 (0755).
    """
    if not name:
      return
    if name == '.':
      return
    if not self.allow_dups_from_deps and name in self.members:
      return

    if mtime is None:
      mtime = self.default_mtime

    # Make directories up the file
    self.conditionally_add_parents(name, mtime=mtime, mode=0o755, uid=uid, gid=gid, uname=uname, gname=gname)

    tarinfo = tarfile.TarInfo(name)
    tarinfo.mtime = mtime
    tarinfo.uid = uid
    tarinfo.gid = gid
    tarinfo.uname = uname
    tarinfo.gname = gname
    tarinfo.type = kind
    if mode is None:
      tarinfo.mode = 0o644 if kind == tarfile.REGTYPE else 0o755
    else:
      tarinfo.mode = mode
    if link:
      tarinfo.linkname = link
    if content:
      content_bytes = content.encode('utf-8')
      tarinfo.size = len(content_bytes)
      self._addfile(tarinfo, io.BytesIO(content_bytes))
    elif file_content:
      with open(file_content, 'rb') as f:
        tarinfo.size = os.fstat(f.fileno()).st_size
        self._addfile(tarinfo, f)
    else:
      self._addfile(tarinfo)

  def add_tar(self,
              tar,
              rootuid=None,
              rootgid=None,
              numeric=False,
              name_filter=None,
              prefix=None):
    """Merge a tar content into the current tar, stripping timestamp.

    Args:
      tar: the name of tar to extract and put content into the current tar.
      rootuid: user id that we will pretend is root (replaced by uid 0).
      rootgid: group id that we will pretend is root (replaced by gid 0).
      numeric: set to true to strip out name of owners (and just use the
          numeric values).
      name_filter: filter out file by names. If not none, this method will be
          called for each file to add, given the name and should return true if
          the file is to be added to the final tar and false otherwise.
      prefix: prefix to add to all file paths.

    Raises:
      TarFileWriter.Error: if an error happens when uncompressing the tar file.
    """
    if prefix:
      prefix = prefix.strip('/') + '/'
    if _DEBUG_VERBOSITY > 1:
      print('==========================  prefix is', prefix)
    intar = tarfile.open(name=tar, mode='r:*')
    for tarinfo in intar:
      if name_filter is None or name_filter(tarinfo.name):
        if not self.preserve_mtime:
          tarinfo.mtime = self.default_mtime
        if rootuid is not None and tarinfo.uid == rootuid:
          tarinfo.uid = 0
          tarinfo.uname = 'root'
        if rootgid is not None and tarinfo.gid == rootgid:
          tarinfo.gid = 0
          tarinfo.gname = 'root'
        if numeric:
          tarinfo.uname = ''
          tarinfo.gname = ''

        in_name = tarinfo.name
        if prefix:
          in_name = os.path.normpath(prefix + in_name).replace(os.path.sep, '/')
        tarinfo.name = in_name
        self.conditionally_add_parents(
            path=tarinfo.name,
            mtime=tarinfo.mtime,
            mode=0o755,
            uid=tarinfo.uid,
            gid=tarinfo.gid,
            uname=tarinfo.uname,
            gname=tarinfo.gname)

        if prefix is not None:
          # Relocate internal hardlinks as well to avoid breaking them.
          link = tarinfo.linkname
          if link.startswith('.') and tarinfo.type == tarfile.LNKTYPE:
            tarinfo.linkname = '.' + prefix + link.lstrip('.')

        # Remove path pax header to ensure that the proposed name is going
        # to be used. Without this, files with long names will not be
        # properly written to its new path.
        if 'path' in tarinfo.pax_headers:
          del tarinfo.pax_headers['path']

        if tarinfo.isfile():
          # use extractfile(tarinfo) instead of tarinfo.name to preserve
          # seek position in intar
          self._addfile(tarinfo, intar.extractfile(tarinfo))
        else:
          self._addfile(tarinfo)
    intar.close()

  def close(self):
    """Close the output tar file.

    This class should not be used anymore after calling that method.

    Raises:
      TarFileWriter.Error: if an error happens when compressing the output file.
    """
    self.tar.close()
    # Close the file object if necessary.
    if self.fileobj:
      self.fileobj.close()
    if self.compressor_proc and self.compressor_proc.wait() != 0:
      raise self.Error('Custom compression command '
                       '"{}" failed'.format(self.compressor_cmd))
