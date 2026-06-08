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
"""Archive reader library for the .deb file testing."""

import io
import os

class SimpleArReader(object):
  """A simple AR file reader.

  This enable to read AR file (System V variant) as described
  in https://en.wikipedia.org/wiki/Ar_(Unix).

  The standard usage of this class is:

  with SimpleArReader(filename) as ar:
    nextFile = ar.next()
    while nextFile:
      print('This archive contains', nextFile.filename)
      nextFile = ar.next()

  Upon error, this class will raise a ArError exception.
  """

  class ArError(Exception):
    pass

  class SimpleArFileEntry(object):
    """Represent one entry in a AR archive.

    Attributes:
      filename: the filename of the entry, as described in the archive.
      timestamp: the timestamp of the file entry.
      owner_id: numeric id of the user and group owning the file.
      group_id: numeric id of the user and group owning the file.
      mode: unix permission mode of the file
      size: size of the file
      data: the content of the file.
    """

    def __init__(self, f):
      self.filename = f.read(16).decode('utf-8').strip()
      if self.filename.endswith('/'):  # SysV variant
        self.filename = self.filename[:-1]
      self.timestamp = int(f.read(12).strip())
      self.owner_id = int(f.read(6).strip())
      self.group_id = int(f.read(6).strip())
      self.mode = int(f.read(8).strip(), 8)
      self.size = int(f.read(10).strip())
      pad = f.read(2)
      if pad != b'\x60\x0a':
        raise SimpleArReader.ArError('Invalid AR file header')
      self.data = f.read(self.size)

  MAGIC_STRING = b'!<arch>\n'

  def __init__(self, filename):
    self.filename = filename

  def __enter__(self):
    self.f = open(self.filename, 'rb')
    if self.f.read(len(self.MAGIC_STRING)) != self.MAGIC_STRING:
      raise self.ArError('Not a ar file: ' + self.filename)
    return self

  def __exit__(self, t, v, traceback):
    self.f.close()

  def next(self):
    """Read the next file. Returns None when reaching the end of file."""
    # AR sections are two bit aligned using new lines.
    if self.f.tell() % 2 != 0:
      self.f.read(1)
    # An AR sections is at least 60 bytes. Some file might contains garbage
    # bytes at the end of the archive, ignore them.
    if self.f.tell() > os.fstat(self.f.fileno()).st_size - 60:
      return None
    return self.SimpleArFileEntry(self.f)
