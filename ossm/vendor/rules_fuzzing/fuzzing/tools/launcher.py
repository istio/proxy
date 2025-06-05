# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Lint as: python3
"""
Wrapper tool providing a uniform flag interface for launching fuzz tests.

This tool acts as a frontend to fuzz test launching. It receives fuzz test
execution parameters as a set of user-provided flags and relays the
configuration to a pluggable backend launcher script via environment
variables. The backend launcher implements the engine-specific logic of
translating the environment variables into the final command that launches
the fuzzer executable.
"""

import os
import shutil
import sys

from absl import app
from absl import flags

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "engine_launcher", None,
    "Path to a shell script that launches the fuzzing engine executable with the appropriate command line arguments."
)

flags.DEFINE_string("binary_path", None, "Path to the fuzz test binary.")

flags.DEFINE_bool(
    "regression", False,
    "If set True, the script will trigger the target as a regression test.")

flags.DEFINE_integer(
    "timeout_secs",
    0,
    "The maximum duration, in seconds, of the fuzzer run launched.",
    lower_bound=0)

flags.DEFINE_string(
    "corpus_dir", None,
    "If non-empty, a directory that will be used as a seed corpus for the fuzzer."
)

flags.DEFINE_string("dictionary_path", None,
                    "If non-empty, a dictionary file of input keywords.")

flags.DEFINE_string(
    "fuzzing_output_root", "/tmp/fuzzing",
    "The root directory for storing all generated artifacts during fuzzing.")

flags.DEFINE_bool(
    "clean", False,
    "If set, cleans up the output directory of the target before fuzzing.")

flags.mark_flag_as_required("engine_launcher")
flags.mark_flag_as_required("binary_path")


def main(argv):
    # TODO(sbucur): Obtain a target-specific path here.
    target_output_root = os.path.join(FLAGS.fuzzing_output_root)
    print("Using test output root: %s" % target_output_root, file=sys.stderr)
    if FLAGS.clean:
        print("Cleaning up the test output root before starting fuzzing...",
              file=sys.stderr)
        try:
            shutil.rmtree(target_output_root)
        except FileNotFoundError:
            pass
    os.makedirs(target_output_root, exist_ok=True)

    corpus_output_path = os.path.join(target_output_root, 'corpus')
    print('Writing new corpus elements at: %s' % corpus_output_path,
          file=sys.stderr)
    os.makedirs(corpus_output_path, exist_ok=True)

    artifacts_output_path = os.path.join(target_output_root, 'artifacts')
    print('Writing new artifacts at: %s' % artifacts_output_path)
    os.makedirs(artifacts_output_path, exist_ok=True)

    os.environ["FUZZER_BINARY"] = FLAGS.binary_path
    os.environ["FUZZER_TIMEOUT_SECS"] = str(FLAGS.timeout_secs)
    os.environ["FUZZER_IS_REGRESSION"] = "1" if FLAGS.regression else "0"
    os.environ["FUZZER_OUTPUT_ROOT"] = target_output_root
    os.environ["FUZZER_OUTPUT_CORPUS_DIR"] = corpus_output_path
    os.environ["FUZZER_ARTIFACTS_DIR"] = artifacts_output_path
    if FLAGS.dictionary_path:
        os.environ["FUZZER_DICTIONARY_PATH"] = FLAGS.dictionary_path
    if FLAGS.corpus_dir:
        os.environ["FUZZER_SEED_CORPUS_DIR"] = FLAGS.corpus_dir
    os.execv("/bin/bash", ["/bin/bash", FLAGS.engine_launcher] + argv[1:])


if __name__ == "__main__":
    app.run(main)
