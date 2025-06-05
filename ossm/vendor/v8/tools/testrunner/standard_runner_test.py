#!/usr/bin/env python3
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Global system tests for V8 test runners and fuzzers.

This hooks up the framework under tools/testrunner testing high-level scenarios
with different test suite extensions and build configurations.
"""

# TODO(machenbach): Mock out util.GuessOS to make these tests really platform
# independent.
# TODO(machenbach): Move coverage recording to a global test entry point to
# include other unittest suites in the coverage report.
# TODO(machenbach): Coverage data from multiprocessing doesn't work.
# TODO(majeski): Add some tests for the fuzzers.

from collections import deque
import os
import sys
import unittest
from os.path import dirname as up
from mock import patch

TOOLS_ROOT = up(up(os.path.abspath(__file__)))
sys.path.append(TOOLS_ROOT)
from testrunner import standard_runner
from testrunner import num_fuzzer
from testrunner.utils.test_utils import (
    temp_base,
    TestRunnerTest,
    with_json_output,
    FakeOSContext,
)

class StandardRunnerTest(TestRunnerTest):
  def get_runner_class(self):
    return standard_runner.StandardTestRunner

  def testPass(self):
    """Test running only passing tests in two variants.

    Also test printing durations.
    """
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default,stress',
        '--time',
        'sweet/bananas',
        'sweet/raspberries',
    )
    result.stdout_includes('sweet/bananas default: PASS')
    # TODO(majeski): Implement for test processors
    # self.assertIn('Total time:', result.stderr, result)
    # self.assertIn('sweet/bananas', result.stderr, result)
    result.has_returncode(0)

  def testPassHeavy(self):
    """Test running with some tests marked heavy."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=nooptimization',
        '-j2',
        'sweet',
        baseroot='testroot3',
    )
    result.stdout_includes('7 tests ran')
    result.has_returncode(0)

  def testShardedProc(self):
    for shard in [1, 2]:
      result = self.run_tests(
          '--progress=verbose',
          '--variants=default,stress',
          '--shard-count=2',
          '--shard-run=%d' % shard,
          'sweet/blackberries',
          'sweet/raspberries',
          infra_staging=False,
      )
      # One of the shards gets one variant of each test.
      result.stdout_includes('2 tests ran')
      if shard == 1:
        result.stdout_includes('sweet/raspberries default')
        result.stdout_includes('sweet/raspberries stress')
        result.has_returncode(0)
      else:
        result.stdout_includes(
          'sweet/blackberries default: FAIL')
        result.stdout_includes(
          'sweet/blackberries stress: FAIL')
        result.has_returncode(1)

  @unittest.skip("incompatible with test processors")
  def testSharded(self):
    """Test running a particular shard."""
    for shard in [1, 2]:
      result = self.run_tests(
          '--progress=verbose',
          '--variants=default,stress',
          '--shard-count=2',
          '--shard-run=%d' % shard,
          'sweet/bananas',
          'sweet/raspberries',
      )
      # One of the shards gets one variant of each test.
      result.stdout_includes('Running 2 tests')
      result.stdout_includes('sweet/bananas')
      result.stdout_includes('sweet/raspberries')
      result.has_returncode(0)

  def testFail(self):
    """Test running only failing tests in two variants."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default,stress',
        'sweet/strawberries',
        infra_staging=False,
    )
    result.stdout_includes('sweet/strawberries default: FAIL')
    result.has_returncode(1)

  def testGN(self):
    """Test running only failing tests in two variants."""
    result = self.run_tests('--gn',baseroot="testroot5")
    result.stdout_includes('>>> Latest GN build found: build')
    result.stdout_includes('Build found: ')
    result.stdout_includes('v8_test_/out.gn/build')
    result.has_returncode(2)

  def testMalformedJsonConfig(self):
    """Test running only failing tests in two variants."""
    result = self.run_tests(baseroot="testroot4")
    result.stdout_includes('contains invalid json')
    result.stdout_includes('Failed to load build config')
    result.stderr_includes('testrunner.base_runner.TestRunnerError')
    result.has_returncode(5)

  def testFailWithRerunAndJSON(self):
    """Test re-running a failing test and output to json."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        '--rerun-failures-count=2',
        '--random-seed=123',
        '--json-test-results', with_json_output,
        'sweet/strawberries',
        infra_staging=False,
    )
    result.stdout_includes('sweet/strawberries default: FAIL')
    # With test processors we don't count reruns as separated failures.
    # TODO(majeski): fix it?
    result.stdout_includes('1 tests failed')
    result.has_returncode(0)

    # TODO(majeski): Previously we only reported the variant flags in the
    # flags field of the test result.
    # After recent changes we report all flags, including the file names.
    # This is redundant to the command. Needs investigation.
    result.json_content_equals('expected_test_results1.json')

  def testFlakeWithRerunAndJSON(self):
    """Test re-running a failing test and output to json."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        '--rerun-failures-count=2',
        '--random-seed=123',
        '--json-test-results', with_json_output,
        'sweet',
        baseroot='testroot2',
        infra_staging=False,
    )
    result.stdout_includes('sweet/bananaflakes default: FAIL PASS')
    result.stdout_includes('=== sweet/bananaflakes (flaky) ===')
    result.stdout_includes('1 tests failed')
    result.stdout_includes('1 tests were flaky')
    result.has_returncode(0)
    result.json_content_equals('expected_test_results2.json')

  def testAutoDetect(self):
    """Fake a build with several auto-detected options.

    Using all those options at once doesn't really make much sense. This is
    merely for getting coverage.
    """
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        'sweet/bananas',
        config_overrides=dict(
          dcheck_always_on=True, is_asan=True, is_cfi=True,
          is_msan=True, is_tsan=True, is_ubsan_vptr=True, target_cpu='x86',
          v8_enable_i18n_support=False, v8_target_cpu='x86',
          v8_enable_verify_csa=False, v8_enable_lite_mode=False,
          v8_enable_pointer_compression=False,
          v8_enable_pointer_compression_shared_cage=False,
          v8_enable_shared_ro_heap=False,
          v8_enable_sandbox=False
        )
    )
    expect_text = (
        '>>> Autodetected:\n'
        'asan\n'
        'cfi_vptr\n'
        'dcheck_always_on\n'
        'msan\n'
        'no_i18n\n'
        'tsan\n'
        'ubsan_vptr\n'
        'webassembly\n'
        '>>> Running tests for ia32.release')
    result.stdout_includes(expect_text)
    result.has_returncode(0)
    # TODO(machenbach): Test some more implications of the auto-detected
    # options, e.g. that the right env variables are set.

  def testLimitedPreloading(self):
    result = self.run_tests('--progress=verbose', '--variants=default', '-j1',
                            'sweet/*')
    result.stdout_includes("sweet/mangoes default: PASS")
    result.stdout_includes("sweet/cherries default: FAIL")
    result.stdout_includes("sweet/apples default: FAIL")
    result.stdout_includes("sweet/strawberries default: FAIL")
    result.stdout_includes("sweet/bananas default: PASS")
    result.stdout_includes("sweet/blackberries default: FAIL")
    result.stdout_includes("sweet/raspberries default: PASS")

  def testWithFakeContext(self):
    with patch(
        'testrunner.local.context.find_os_context_factory',
        return_value=FakeOSContext):
      result = self.run_tests(
          '--progress=verbose',
          'sweet',
      )
      result.stdout_includes('===>Starting stuff\n'
                             '>>> Running tests for x64.release\n'
                             '>>> Running with test processors\n')
      result.stdout_includes('--- stdout ---\nfake stdout 1')
      result.stdout_includes('--- stderr ---\nfake stderr 1')
      result.stdout_includes('=== sweet/raspberries ===')
      result.stdout_includes('=== sweet/cherries ===')
      result.stdout_includes('=== sweet/apples ===')
      result.stdout_includes('Command: fake_wrapper ')
      result.stdout_includes(
          '===\n'
          '=== 4 tests failed\n'
          '===\n'
          '>>> 7 base tests produced 7 (100%) non-filtered tests\n'
          '>>> 7 tests ran\n'
          '<===Stopping stuff\n')

  def testSkips(self):
    """Test skipping tests in status file for a specific variant."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=nooptimization',
        'sweet/strawberries',
        infra_staging=False,
    )
    result.stdout_includes('0 tests ran')
    result.has_returncode(2)

  def testRunSkips(self):
    """Inverse the above. Test parameter to keep running skipped tests."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=nooptimization',
        '--run-skipped',
        'sweet/strawberries',
    )
    result.stdout_includes('1 tests failed')
    result.stdout_includes('1 tests ran')
    result.has_returncode(1)

  def testDefault(self):
    """Test using default test suites, though no tests are run since they don't
    exist in a test setting.
    """
    result = self.run_tests(
        infra_staging=False,
    )
    result.stdout_includes('0 tests ran')
    result.has_returncode(2)

  def testNoBuildConfig(self):
    """Test failing run when build config is not found."""
    result = self.run_tests(baseroot='wrong_path')
    result.stdout_includes('Failed to load build config')
    result.has_returncode(5)

  def testInconsistentArch(self):
    """Test failing run when attempting to wrongly override the arch."""
    result = self.run_tests('--arch=ia32')
    result.stdout_includes(
        '--arch value (ia32) inconsistent with build config (x64).')
    result.has_returncode(5)

  def testWrongVariant(self):
    """Test using a bogus variant."""
    result = self.run_tests('--variants=meh')
    result.has_returncode(5)

  def testModeFromBuildConfig(self):
    """Test auto-detection of mode from build config."""
    result = self.run_tests('--outdir=out/build', 'sweet/bananas')
    result.stdout_includes('Running tests for x64.release')
    result.has_returncode(0)

  def testPredictable(self):
    """Test running a test in verify-predictable mode.

    The test will fail because of missing allocation output. We verify that and
    that the predictable flags are passed and printed after failure.
    """
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        'sweet/bananas',
        infra_staging=False,
        config_overrides=dict(v8_enable_verify_predictable=True),
    )
    result.stdout_includes('1 tests ran')
    result.stdout_includes('sweet/bananas default: FAIL')
    result.stdout_includes('Test had no allocation output')
    result.stdout_includes('--predictable --verify-predictable')
    result.has_returncode(1)

  def testSlowArch(self):
    """Test timeout factor manipulation on slow architecture."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        'sweet/bananas',
        config_overrides=dict(v8_target_cpu='arm64'),
    )
    # TODO(machenbach): We don't have a way for testing if the correct
    # timeout was used.
    result.has_returncode(0)

  def testRandomSeedStressWithDefault(self):
    """Test using random-seed-stress feature has the right number of tests."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        '--random-seed-stress-count=2',
        'sweet/bananas',
        infra_staging=False,
    )
    result.stdout_includes('2 tests ran')
    result.has_returncode(0)

  def testRandomSeedStressWithSeed(self):
    """Test using random-seed-stress feature passing a random seed."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        '--random-seed-stress-count=2',
        '--random-seed=123',
        'sweet/strawberries',
    )
    result.stdout_includes('2 tests ran')
    # We use a failing test so that the command is printed and we can verify
    # that the right random seed was passed.
    result.stdout_includes('--random-seed=123')
    result.has_returncode(1)

  def testSpecificVariants(self):
    """Test using NO_VARIANTS modifiers in status files skips the desire tests.

    The test runner cmd line configures 4 tests to run (2 tests * 2 variants).
    But the status file applies a modifier to each skipping one of the
    variants.
    """
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default,stress',
        'sweet/bananas',
        'sweet/raspberries',
        config_overrides=dict(is_asan=True),
    )
    # Both tests are either marked as running in only default or only
    # slow variant.
    result.stdout_includes('2 tests ran')
    result.has_returncode(0)

  def testDotsProgress(self):
    result = self.run_tests(
        '--progress=dots',
        'sweet/cherries',
        'sweet/bananas',
        '--no-sorting', '-j1', # make results order deterministic
        infra_staging=False,
    )
    result.stdout_includes('2 tests ran')
    result.stdout_includes('F.')
    result.has_returncode(1)

  def testMonoProgress(self):
    self._testCompactProgress('mono')

  def testColorProgress(self):
    self._testCompactProgress('color')

  def _testCompactProgress(self, name):
    result = self.run_tests(
        '--progress=%s' % name,
        'sweet/cherries',
        'sweet/bananas',
        infra_staging=False,
    )
    if name == 'color':
      expected = ('\033[34m%  28\033[0m|'
                  '\033[32m+   1\033[0m|'
                  '\033[31m-   1\033[0m]: Done')
    else:
      expected = '%  28|+   1|-   1]: Done'
    result.stdout_includes(expected)
    result.stdout_includes('sweet/cherries')
    result.stdout_includes('sweet/bananas')
    result.has_returncode(1)

  def testCompactErrorDetails(self):
    result = self.run_tests(
        '--progress=mono',
        'sweet/cherries',
        'sweet/strawberries',
        infra_staging=False,
    )
    result.stdout_includes('sweet/cherries')
    result.stdout_includes('sweet/strawberries')
    result.stdout_includes('+Mock diff')
    result.has_returncode(1)

  def testExitAfterNFailures(self):
    result = self.run_tests(
        '--progress=verbose',
        '--exit-after-n-failures=2',
        '-j1',
        'sweet/mangoes',       # PASS
        'sweet/strawberries',  # FAIL
        'sweet/blackberries',  # FAIL
        'sweet/raspberries',   # should not run
    )
    result.stdout_includes('sweet/mangoes default: PASS')
    result.stdout_includes('sweet/strawberries default: FAIL')
    result.stdout_includes('Too many failures, exiting...')
    result.stdout_includes('sweet/blackberries default: FAIL')
    result.stdout_excludes('sweet/raspberries')
    result.stdout_includes('2 tests failed')
    result.stdout_includes('3 tests ran')
    result.has_returncode(1)

  def testHeavySequence(self):
    """Test a configuration with 2 heavy tests.
    One heavy test will get buffered before being run.
    """
    appended = 0
    popped = 0

    def mock_deque():

      class MockQ():

        def __init__(self):
          self.buffer = deque()

        def append(self, test):
          nonlocal appended
          self.buffer.append(test)
          appended += 1

        def popleft(self):
          nonlocal popped
          popped += 1
          return self.buffer.popleft()

        def __bool__(self):
          return bool(self.buffer)

      return MockQ()

    # Swarming option will trigger a cleanup routine on the bot
    def mock_kill():
      pass

    with patch('testrunner.testproc.sequence.deque', side_effect=mock_deque), \
         patch('testrunner.testproc.util.kill_processes_linux', side_effect=mock_kill):
      result = self.run_tests(
          '--variants=default', '--swarming', 'fat', baseroot="testroot6")

      result.has_returncode(1)
      self.assertEqual(1, appended)
      self.assertEqual(1, popped)

  def testRunnerFlags(self):
    """Test that runner-specific flags are passed to tests."""
    result = self.run_tests(
        '--progress=verbose',
        '--variants=default',
        '--random-seed=42',
        'sweet/bananas',
        '-v',
    )

    result.stdout_includes(
        '--test bananas --random-seed=42 --nohard-abort --testing-d8-test-runner')
    result.has_returncode(0)


class NumFuzzerTest(TestRunnerTest):
  def get_runner_class(self):
    return num_fuzzer.NumFuzzer

  def testNumFuzzer(self):
    result = self.run_tests(
      '--command-prefix', sys.executable,
      '--outdir', 'out/build',
    )
    result.has_returncode(0)
    result.stdout_includes('>>> Autodetected')

class OtherTest(TestRunnerTest):
  def testStatusFilePresubmit(self):
    """Test that the fake status file is well-formed."""
    with temp_base() as basedir:
      from testrunner.local import statusfile
      self.assertTrue(statusfile.PresubmitCheck(
          os.path.join(basedir, 'test', 'sweet', 'sweet.status')))

if __name__ == '__main__':
  unittest.main()
