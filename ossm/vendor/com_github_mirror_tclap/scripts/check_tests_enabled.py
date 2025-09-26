#!/usr/bin/python

# Copyright (c) 2017 Google Inc.
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

ignored_files = frozenset(['testCheck.sh'])

def get_test_files(path):
    return map(os.path.basename,
               glob.glob(os.path.join(path, 'test*.sh')))

def get_test_cases(path):
    with file(os.path.join(path, 'Makefile.am')) as f:
        lines = []
        for line in f:
            m = re.match(r'.*(test\d+\.sh).*', line.strip())
            if m:
                lines.append(m.group(1))

        return lines

def main():
    test_files = set(get_test_files('./tests')) - ignored_files
    test_cases = set(get_test_cases('./tests'))
    diff = test_files - test_cases
    if diff:
        print 'The following files are not in Makefile.am'
        print diff
        sys.exit(1)

if __name__ == '__main__':
    main()
    
