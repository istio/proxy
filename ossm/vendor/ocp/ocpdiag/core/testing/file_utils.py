# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""File-finding utilities."""
import os

_GOOGLE_WORKSPACE = 'google3'


def data_file_path_prefix():
  """Retrieves the root path for all data dependencies.

  In bazel, the TEST_WORKSPACE variable is appended to the SRCDIR, whereas
  in blaze it's ignored.

  Returns:
    The root path string to the data dependency tree.
  """
  test_srcdir = os.environ.get('TEST_SRCDIR')
  test_workspace = os.environ.get('TEST_WORKSPACE')
  if test_workspace == _GOOGLE_WORKSPACE:
    return test_srcdir
  return '{:s}/{:s}'.format(test_srcdir, test_workspace)
