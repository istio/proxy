# Copyright 2017 The Bazel Authors. All rights reserved.
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
"""A simple cross-platform helper to create an RPM package."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import contextlib
import fileinput
import os
import pprint
import re
import shutil
import subprocess
import sys
import tempfile
from string import Template

from pkg.private import helpers


# Setup to safely create a temporary directory and clean it up when done.
@contextlib.contextmanager
def Cd(newdir, cleanup=lambda: True):
  """Change the current working directory.

  This will run the provided cleanup function when the context exits and the
  previous working directory is restored.

  Args:
    newdir: The directory to change to. This must already exist.
    cleanup: An optional cleanup function to be executed when the context exits.

  Yields:
    Nothing.
  """

  prevdir = os.getcwd()
  os.chdir(os.path.expanduser(newdir))
  try:
    yield
  finally:
    os.chdir(prevdir)
    cleanup()


@contextlib.contextmanager
def Tempdir():
  """Create a new temporary directory and change to it.

  The temporary directory will be removed when the context exits.

  Yields:
    The full path of the temporary directory.
  """

  dirpath = tempfile.mkdtemp()

  def Cleanup():
    shutil.rmtree(dirpath)

  with Cd(dirpath, Cleanup):
    yield dirpath


WROTE_FILE_RE = re.compile(r'Wrote: (?P<rpm_path>.+)', re.MULTILINE)


def FindOutputFile(log):
  """Find the written file from the log information."""
  m = WROTE_FILE_RE.findall(log)
  if m:
    return m
  return None

def SlurpFile(input_path):
  with open(input_path, 'r') as input:
    return input.read()

def CopyAndRewrite(input_file, output_file, replacements=None, template_replacements=None):
  """Copies the given file and optionally rewrites with replacements.

  Args:
    input_file: The file to copy.

    output_file: The file to write to.

    replacements: A dictionary of replacements.
      Keys are prefixes scan for, values are the replacements to write after
      the prefix.

    template_replacements: A dictionary of in-place replacements.
      Keys are variable names, values are replacements.  Used with
      string.Template.
  """

  with open(output_file, 'w') as output:
    for line in fileinput.input(input_file):
      if replacements:
        for prefix, text in replacements.items():
          if line.startswith(prefix):
            line = prefix + ' ' + text + '\n'
            break
      if template_replacements:
        template = Template(line)
        line = template.safe_substitute(template_replacements)
      output.write(line)


def IsExe(fpath):
  return os.path.isfile(fpath) and os.access(fpath, os.X_OK)


def Which(program):
  """Search for the given program in the PATH.

  Args:
    program: The program to search for.

  Returns:
    The full path to the program.
  """

  for path in os.environ['PATH'].split(os.pathsep):
    filename = os.path.join(path, program)
    if IsExe(filename):
      return filename

  return None


class NoRpmbuildFoundError(Exception):
  pass


class InvalidRpmbuildError(Exception):
  pass


def FindRpmbuild(rpmbuild_path):
  """Finds absolute path to rpmbuild.

  Args:
    rpmbuild_path: path to the rpmbuild_binary. If None, find 'rpmbuild' by
                   walking $PATH.
  """
  if rpmbuild_path:
    if not rpmbuild_path.startswith(os.path.sep):
      return os.path.join(os.getcwd(), rpmbuild_path)
    return rpmbuild_path
  path = Which('rpmbuild')
  if path:
    return path
  raise NoRpmbuildFoundError()


class RpmBuilder(object):
  """A helper class to manage building the RPM file."""

  SOURCE_DIR = 'SOURCES'
  BUILD_DIR = 'BUILD'
  BUILD_SUBDIR = 'BUILD_SUB'
  BUILDROOT_DIR = 'BUILDROOT'
  TEMP_DIR = 'TMP'
  RPMS_DIR = 'RPMS'
  DIRS = [SOURCE_DIR, BUILD_DIR, RPMS_DIR, TEMP_DIR]

  # `debuginfo` RPM types as defined in `toolchains/rpm/rpmbuild_configure.bzl`
  DEBUGINFO_TYPE_NONE = "none"
  DEBUGINFO_TYPE_CENTOS = "centos"
  DEBUGINFO_TYPE_FEDORA = "fedora"
  SUPPORTED_DEBUGINFO_TYPES = {
    DEBUGINFO_TYPE_CENTOS,
    DEBUGINFO_TYPE_FEDORA,
  }

  def __init__(self, name, version, release, arch, rpmbuild_path,
               source_date_epoch=None,
               debug=False):
    self.name = name
    self.version = helpers.GetFlagValue(version)
    self.release = helpers.GetFlagValue(release)
    self.arch = arch
    self.files = []
    self.rpmbuild_path = FindRpmbuild(rpmbuild_path)
    self.rpm_paths = None
    self.source_date_epoch = helpers.GetFlagValue(source_date_epoch)
    self.debug = debug

    # The below are initialized in SetupWorkdir()
    self.spec_file = None

    self.preamble_file = None
    self.description_file = None
    self.install_script_file = None
    self.file_list_path = None
    self.changelog = None

    self.pre_scriptlet = None
    self.post_scriptlet = None
    self.preun_scriptlet = None
    self.postun_scriptlet = None
    self.subrpms = None

  def AddFiles(self, paths, root=''):
    """Add a set of files to the current RPM.

    If an item in paths is a directory, its files are recursively added.

    Args:
      paths: The files to add.
      root: The root of the filesystem to search for files. Defaults to ''.
    """
    for path in paths:
      full_path = os.path.join(root, path)
      if os.path.isdir(full_path):
        self.AddFiles(os.listdir(full_path), full_path)
      else:
        self.files.append(full_path)

  def SetupWorkdir(self,
                   spec_file,
                   original_dir,
                   preamble_file=None,
                   description_file=None,
                   install_script_file=None,
                   subrpms_file=None,
                   pre_scriptlet_path=None,
                   post_scriptlet_path=None,
                   preun_scriptlet_path=None,
                   postun_scriptlet_path=None,
                   posttrans_scriptlet_path=None,
                   changelog_file=None,
                   file_list_path=None):
    """Create the needed structure in the workdir."""

    # Create the rpmbuild-expected directory structure.
    for name in RpmBuilder.DIRS:
      if not os.path.exists(name):
        os.makedirs(name, 0o777)

    # Copy the to-be-packaged files into the BUILD directory
    for f in self.files:
      dst_dir = os.path.join(RpmBuilder.BUILD_DIR, os.path.dirname(f))
      if not os.path.exists(dst_dir):
        os.makedirs(dst_dir, 0o777)
      shutil.copy(os.path.join(original_dir, f), dst_dir)

    # The code below is related to assembling the RPM spec template and
    # everything else it needs to produce a valid RPM package.
    #
    # There two different types of substitution going on here: textual, directly
    # into the spec file, and macro; done when we call rpmbuild(8).
    #
    # Plans to clean this up are tracked in #209.

    # Slurp in the scriptlets...
    self.pre_scriptlet = \
      SlurpFile(os.path.join(original_dir, pre_scriptlet_path)) if pre_scriptlet_path else ''
    self.post_scriptlet = \
      SlurpFile(os.path.join(original_dir, post_scriptlet_path)) if post_scriptlet_path else ''
    self.preun_scriptlet = \
      SlurpFile(os.path.join(original_dir, preun_scriptlet_path)) if preun_scriptlet_path else ''
    self.postun_scriptlet = \
      SlurpFile(os.path.join(original_dir, postun_scriptlet_path)) if postun_scriptlet_path else ''
    self.posttrans_scriptlet = \
      SlurpFile(os.path.join(original_dir, posttrans_scriptlet_path)) if posttrans_scriptlet_path else ''
    self.subrpms = \
      SlurpFile(os.path.join(original_dir, subrpms_file)) if subrpms_file else ''

    # Then prepare for textual substitution.  This is typically only the case for the
    # experimental `pkg_rpm`.
    tpl_replacements = {
      'PRE_SCRIPTLET': ("%pre\n" + self.pre_scriptlet) if self.pre_scriptlet else "",
      'POST_SCRIPTLET': ("%post\n" + self.post_scriptlet) if self.post_scriptlet else "",
      'PREUN_SCRIPTLET': ("%preun\n" + self.preun_scriptlet) if self.preun_scriptlet else "",
      'POSTUN_SCRIPTLET': ("%postun\n" + self.postun_scriptlet) if self.postun_scriptlet else "",
      'POSTTRANS_SCRIPTLET': ("%posttrans\n" + self.posttrans_scriptlet) if self.posttrans_scriptlet else "",
      'SUBRPMS' : self.subrpms,
      'CHANGELOG': ""
    }

    if changelog_file:
      self.changelog = SlurpFile(os.path.join(original_dir, changelog_file))
      tpl_replacements["CHANGELOG"] = "%changelog\n" + self.changelog

    # If the spec file has "Version" and "Release" tags specified in the spec
    # file's preamble, the values are filled in immediately afterward.  These go
    # into "replacements".  This is typically only the case for the "original"
    # `pkg_rpm`.
    #
    # The "tpl_replacements" are used for direct text substitution of scriptlets
    # into the spec file, typically only for the "experimental" `pkg_rpm`.
    spec_origin = os.path.join(original_dir, spec_file)
    self.spec_file = os.path.basename(spec_file)
    replacements = {}
    if self.version:
      replacements['Version:'] = self.version
    if self.release:
      replacements['Release:'] = self.release
    CopyAndRewrite(spec_origin, self.spec_file,
                   replacements=replacements,
                   template_replacements=tpl_replacements)

    # "Preamble" template substitutions.  Currently only support values for the
    # "Version" and "Release" tags.
    #
    # This is only the case for `pkg_rpm` in experimental/rpm.bzl.
    #
    # This is substituted by rpmbuild(8) via macro expansion.
    if preamble_file:
      # Copy in the various other files needed to build the RPM
      self.preamble_file = os.path.basename(preamble_file)
      tpl_replacements = {}
      if self.version:
        tpl_replacements['VERSION_FROM_FILE'] = self.version
      if self.release:
        tpl_replacements['RELEASE_FROM_FILE'] = self.release
      CopyAndRewrite(os.path.join(original_dir, preamble_file),
                     self.preamble_file,
                     template_replacements=tpl_replacements)

    # The below are all copied into place within the RPM spec root.  It may be
    # possible to directly some, if not all, of these out of the Bazel build
    # root instead. "file_list_path" may be the problematic one here,
    # as it must be there.
    #
    # These are substituted by rpmbuild(8) via macro expansion.

    # Used in %description
    if description_file:
      shutil.copy(os.path.join(original_dir, description_file), os.getcwd())
      self.description_file = os.path.basename(description_file)

    # Used in %install
    if install_script_file:
      shutil.copy(os.path.join(original_dir, install_script_file), os.getcwd())
      self.install_script_file = os.path.basename(install_script_file)

    # Used in %files -f
    if file_list_path:
      shutil.copy(os.path.join(original_dir, file_list_path), RpmBuilder.BUILD_DIR)
      self.file_list_path = os.path.join(RpmBuilder.BUILD_DIR, os.path.basename(file_list_path))

  def CallRpmBuild(self, dirname, rpmbuild_args, debuginfo_type):
    """Call rpmbuild with the correct arguments."""

    buildroot = os.path.join(dirname, RpmBuilder.BUILDROOT_DIR)
    # For reference, E121 is a hanging indent flake8 issue.  It really wants
    # four space indents, but properly fixing that will require re-indenting the
    # entire file.

    # Further, the use of disabling yapf and friends is to allow argument names
    # to be associated with their values neatly.
    args = [
        self.rpmbuild_path,  # noqa: E121
    ]
    if self.debug:
      args.append('-vv')

    if debuginfo_type == RpmBuilder.DEBUGINFO_TYPE_FEDORA:
      os.makedirs(f'{dirname}/{RpmBuilder.BUILD_DIR}/{RpmBuilder.BUILD_SUBDIR}')

    # Common options
    # NOTE: There may be a need to add '--define', 'buildsubdir .' for some
    # rpmbuild versions. But that breaks other rpmbuild versions, so before
    # adding it back in, add extensive tests.
    args += [
      '--define', '_topdir %s' % dirname,
      '--define', '_tmppath %s/TMP' % dirname,
      '--define', '_builddir %s/BUILD' % dirname,
    ]

    if debuginfo_type in RpmBuilder.SUPPORTED_DEBUGINFO_TYPES:
      args += ['--undefine', '_debugsource_packages']

    if debuginfo_type == RpmBuilder.DEBUGINFO_TYPE_CENTOS:
      args += ['--define', 'buildsubdir .']

    if debuginfo_type == RpmBuilder.DEBUGINFO_TYPE_FEDORA:
      args += ['--define', f'buildsubdir {RpmBuilder.BUILD_SUBDIR}']

    args += [
      '--bb',
      '--buildroot=%s' % buildroot,
    ]  # yapf: disable

    # Macro-based RPM parameter substitution, if necessary inputs provided.
    if self.preamble_file:
      args += ['--define', 'build_rpm_options %s' % self.preamble_file]
    if self.description_file:
      args += ['--define', 'build_rpm_description %s' % self.description_file]
    if self.install_script_file:
      args += ['--define', 'build_rpm_install %s' % self.install_script_file]
    if self.file_list_path:
      # %files -f is taken relative to the package root
      base_path = os.path.basename(self.file_list_path)
      if debuginfo_type == RpmBuilder.DEBUGINFO_TYPE_FEDORA:
        base_path = os.path.join("..", base_path)

      args += ['--define', 'build_rpm_files %s' % base_path]

    args.extend(rpmbuild_args)

    args.append(self.spec_file)

    env = {
        'LANG': 'C',
        'RPM_BUILD_ROOT': buildroot,
    }

    if self.source_date_epoch is not None:
      env['SOURCE_DATE_EPOCH'] = self.source_date_epoch
      args += ["--define", "clamp_mtime_to_source_date_epoch 1"]
      args += ["--define", "use_source_date_epoch_as_buildtime 1"]

    if self.debug:
      print('Running rpmbuild as:', ' '.join(["'" + a + "'" for a in args]))
      print('With environment:')
      pprint.pprint(env)

    p = subprocess.Popen(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env)
    output = p.communicate()[0].decode()

    if p.returncode == 0:
      # Find the created file.
      self.rpm_paths = FindOutputFile(output)

    if p.returncode != 0 or not self.rpm_paths:
      print('Error calling rpmbuild:')
      print(output)
    elif self.debug:
      print(output)

    # Return the status.
    return p.returncode

  def SaveResult(self, out_file, subrpm_out_files):
    """Save the result RPM out of the temporary working directory."""
    if self.rpm_paths:
      subrpms_seen = set()
      sorted_subrpm_out_files = sorted(
         subrpm_out_files, key=lambda n: len(n[1]), reverse=True)

      for p in self.rpm_paths:
         is_subrpm = False

         for subrpm_name, subrpm_out_file in sorted_subrpm_out_files:
            if subrpm_name in subrpms_seen:
               continue

            subrpm_prefix = self.name + '-' + subrpm_name
            if os.path.basename(p).startswith(subrpm_prefix):
               subrpms_seen.add(subrpm_name)
               shutil.copy(p, subrpm_out_file)
               is_subrpm = True
               if self.debug:
                  print('Saved %s sub RPM file to %s' % (
                     subrpm_name, subrpm_out_file))
               break

         if not is_subrpm:
            shutil.copy(p, out_file)
            if self.debug:
               print('Saved RPM file to %s' % out_file)
    else:
      print('No RPM file created.')

  def Build(self, spec_file, out_file, subrpm_out_files=None,
            preamble_file=None,
            description_file=None,
            install_script_file=None,
            subrpms_file=None,
            pre_scriptlet_path=None,
            post_scriptlet_path=None,
            preun_scriptlet_path=None,
            postun_scriptlet_path=None,
            posttrans_scriptlet_path=None,
            file_list_path=None,
            changelog_file=None,
            rpmbuild_args=None,
            debuginfo_type=None):
    """Build the RPM described by the spec_file, with other metadata in keyword arguments"""

    if self.debug:
      print('Building RPM for %s at %s' % (self.name, out_file))

    original_dir = os.getcwd()
    spec_file = os.path.join(original_dir, spec_file)
    out_file = os.path.join(original_dir, out_file)

    if subrpm_out_files:
      subrpm_out_files = (s.split(':') for s in subrpm_out_files)
      subrpm_out_files = [
         (s[0], os.path.join(original_dir, s[1])) for s in subrpm_out_files]
    else:
      subrpm_out_files = []

    with Tempdir() as dirname:
      self.SetupWorkdir(spec_file,
                        original_dir,
                        preamble_file=preamble_file,
                        description_file=description_file,
                        install_script_file=install_script_file,
                        subrpms_file=subrpms_file,
                        file_list_path=file_list_path,
                        pre_scriptlet_path=pre_scriptlet_path,
                        post_scriptlet_path=post_scriptlet_path,
                        preun_scriptlet_path=preun_scriptlet_path,
                        postun_scriptlet_path=postun_scriptlet_path,
                        posttrans_scriptlet_path=posttrans_scriptlet_path,
                        changelog_file=changelog_file)
      status = self.CallRpmBuild(dirname, rpmbuild_args or [], debuginfo_type)
      self.SaveResult(out_file, subrpm_out_files)

    return status


def main(argv):
  parser = argparse.ArgumentParser(
      description='Helper for building rpm packages',
      fromfile_prefix_chars='@')

  parser.add_argument('--name',
                      help='The name of the software being packaged.')
  parser.add_argument('--version',
                      help='The version of the software being packaged.')
  parser.add_argument('--release',
                      help='The release of the software being packaged.')
  parser.add_argument(
      '--arch',
      help='The CPU architecture of the software being packaged.')
  parser.add_argument('--spec_file', required=True,
                      help='The file containing the RPM specification.')
  parser.add_argument('--out_file', required=True,
                      help='The destination to save the resulting RPM file to.')
  parser.add_argument('--subrpm_out_file', action='append',
                      help='List of destinations to save resulting ' +
                      'Sub RPMs to in the form of name:destination')
  parser.add_argument('--rpmbuild', help='Path to rpmbuild executable.')
  parser.add_argument('--source_date_epoch',
                      help='Value for the SOURCE_DATE_EPOCH rpmbuild '
                           'environment variable')
  parser.add_argument('--debug', action='store_true', default=False,
                      help='Print debug messages.')

  # Options currently used experimental/rpm.bzl:
  parser.add_argument('--install_script',
                      help='Installer script')
  parser.add_argument('--file_list',
                      help='File containing a list of files to include with rpm spec %%files -f')
  parser.add_argument('--preamble',
                      help='File containing the RPM Preamble')
  parser.add_argument('--description',
                      help='File containing the RPM %%description text')
  parser.add_argument('--subrpms',
                      help='File containing the RPM subrpm details')
  parser.add_argument('--pre_scriptlet',
                      help='File containing the RPM %%pre scriptlet, if to be substituted')
  parser.add_argument('--post_scriptlet',
                      help='File containing the RPM %%post scriptlet, if to be substituted')
  parser.add_argument('--preun_scriptlet',
                      help='File containing the RPM %%preun scriptlet, if to be substituted')
  parser.add_argument('--postun_scriptlet',
                      help='File containing the RPM %%postun scriptlet, if to be substituted')
  parser.add_argument('--posttrans_scriptlet',
                      help='File containing the RPM %%posttrans scriptlet, if to be substituted')
  parser.add_argument('--changelog',
                      help='File containing the RPM changelog text')

  parser.add_argument('--rpmbuild_arg', dest='rpmbuild_args', action='append',
                      help='Any additional arguments to pass to rpmbuild')
  parser.add_argument('--debuginfo_type', default=RpmBuilder.DEBUGINFO_TYPE_NONE,
                      choices=sorted(RpmBuilder.SUPPORTED_DEBUGINFO_TYPES) + [RpmBuilder.DEBUGINFO_TYPE_NONE],
                      help='debuginfo type to use')
  parser.add_argument('files', nargs='*')

  options = parser.parse_args(argv or ())

  try:
    builder = RpmBuilder(options.name,
                         options.version, options.release,
                         options.arch, options.rpmbuild,
                         source_date_epoch=options.source_date_epoch,
                         debug=options.debug)
    builder.AddFiles(options.files)
    return builder.Build(options.spec_file, options.out_file,
                         options.subrpm_out_file,
                         preamble_file=options.preamble,
                         description_file=options.description,
                         install_script_file=options.install_script,
                         subrpms_file=options.subrpms,
                         file_list_path=options.file_list,
                         pre_scriptlet_path=options.pre_scriptlet,
                         post_scriptlet_path=options.post_scriptlet,
                         preun_scriptlet_path=options.preun_scriptlet,
                         postun_scriptlet_path=options.postun_scriptlet,
                         posttrans_scriptlet_path=options.posttrans_scriptlet,
                         changelog_file=options.changelog,
                         rpmbuild_args=options.rpmbuild_args,
                         debuginfo_type=options.debuginfo_type)
  except NoRpmbuildFoundError:
    print('ERROR: rpmbuild is required but is not present in PATH')
    return 1


if __name__ == '__main__':
  main(sys.argv[1:])

# vim: ts=2:sw=2:
