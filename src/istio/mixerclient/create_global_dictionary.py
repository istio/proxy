#!/usr/bin/env python
#
# Copyright 2017 Istio Authors. All Rights Reserved.
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

import sys

TOP = r"""/* Copyright 2017 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/istio/mixerclient/global_dictionary.h"

namespace istio {
namespace mixerclient {
namespace {

/*
 * Automatically generated global dictionary from
 * https://github.com/istio/api/blob/master/mixer/v1/global_dictionary.yaml
 * by run:
 *  ./create_global_dictionary.py \
 *    bazel-mixerclient/external/mixerapi_git/mixer/v1/global_dictionary.yaml \
 *      > src/global_dictionary.cc
 */
"""

INDENT = "    "

FORWORD_INDEX_START = """const std::vector<std::string> kGlobalWords{
""" + INDENT
FORWORD_INDEX_END = "};\n"

REVERSE_INDEX_START = """const std::unordered_map<std::string, int> kGlobalDictionary {
""" + INDENT
REVERSE_INDEX_END = "};"


BOTTOM = r"""
}  // namespace

const std::vector<std::string>& GetGlobalWords() { return kGlobalWords; }
const std::unordered_map<std::string, int>& GetGlobalDictionary() {
  return kGlobalDictionary;
}
}  // namespace mixerclient
}  // namespace istio"""

all_words = []
word_pairs = []
index = 0
with open(sys.argv[1]) as src_file:
    for line in src_file:
        if line.startswith("-"):
            # 'word"' => '"word\""'
            string_literal = "\"" + line[1:].strip().replace("\"", "\\\"") + "\""
            # 'word' => '   "word"'
            all_words.append(INDENT + string_literal)
            # '   { "word", 1}'
            word_pairs.append(INDENT + "{" + string_literal + "," + str(index) + "}")
            index = index + 1
all_words_literal = ",\n".join(all_words)
word_pairs_literal = ",\n".join(word_pairs)
print (TOP
       + FORWORD_INDEX_START + all_words_literal + FORWORD_INDEX_END
       + REVERSE_INDEX_START + word_pairs_literal + REVERSE_INDEX_END
       + BOTTOM)
