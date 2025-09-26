# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Lint as: python3
"""
Validates the fuzzing dictionary.
"""

import re

_DICTIONARY_LINE_RE = re.compile(
    r'''[^"]*  # Skip an arbitrary prefix (not used by libFuzzer).
        "      # Must be enclosed in quotes.
        (         # A space or printable character in "C" locale except `\`...
         ((?!\\)[\f\r\t\v\x20-\x7e])
        |
         \\(      # ...or an escape sequence...
            [\\\"]  # ...consisting of either `\` or `"`...
           |
            x[0-9a-f]{2}  # ...or a hexa number, e.g. '\x0f'
           )
        )+
        "
        \s*  # Skip any space after the entry.''',
    flags=re.IGNORECASE | re.VERBOSE)

def validate_line(line):
    """Validates a single line in the fuzzing dictionary entry.

    Args:
        line: a string containing a single line in the fuzzing dictionary.

    Returns:
        True if the argument is allowed to exist in a fuzzing dictionary, 
        otherwise False.
    """
    line = line.strip()
    if not line or line.startswith('#'):
        return True
    else:
        return re.fullmatch(_DICTIONARY_LINE_RE, line) is not None
