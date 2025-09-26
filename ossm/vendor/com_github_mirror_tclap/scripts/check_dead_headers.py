#!/usr/bin/python

# Copyright (c) 2018 Google LLC
# All rights reserved.
#
# See the file COPYING in the top directory of this distribution for
# more information.
#
# THE SOFTWARE IS PROVIDED _AS IS_, WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

import glob
import os
import sys
import re

def get_files(path, pattern):
    return map(os.path.basename,
               glob.glob(os.path.join(path, pattern)))

def get_includes(path):
    with file(os.path.join(path, 'Makefile.am')) as f:
        lines = []

        for line in f:
            m = re.match(r'^(.+\.h)', line.strip())
            if m:
                lines.append(m.group(1))

        return lines

def main():
    headers = set(get_files('./include/tclap', '*.h'))
    includes = set(get_includes('./include/tclap'))
    diff = headers - includes
    if diff:
        print 'The following files are not in Makefile.am'
        print diff
        sys.exit(1)

if __name__ == '__main__':
    main()
