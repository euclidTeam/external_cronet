#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import glob
import json
import os
import pipes
import platform
import re
import shutil
import stat
import subprocess
import sys

from gn_helpers import ToGNString

# VS 2019 16.61 with 10.0.20348.0 SDK, 10.0.19041 version of Debuggers
# with ARM64 libraries and UWP support.
# See go/chromium-msvc-toolchain for instructions about how to update the
# toolchain.
#
# When updating the toolchain, consider the following areas impacted by the
# toolchain version:
#
# * //base/win/windows_version.cc NTDDI preprocessor check
#   Triggers a compiler error if the available SDK is older than the minimum.
# * //build/config/win/BUILD.gn NTDDI_VERSION value
#   Affects the availability of APIs in the toolchain headers.
# * //docs/windows_build_instructions.md mentions of VS or Windows SDK.
#   Keeps the document consistent with the toolchain version.
TOOLCHAIN_HASH = '1023ce2e82'

script_dir = os.path.dirname(os.path.realpath(__file__))
json_data_file = os.path.join(script_dir, 'win_toolchain.json')

# VS versions are listed in descending order of priority (highest first).
# The first version is assumed by this script to be the one that is packaged,
# which makes a difference for the arm64 runtime.
MSVS_VERSIONS = collections.OrderedDict([
    ('2019', '16.0'),  # Default and packaged version of Visual Studio.
    ('2022', '17.0'),
    ('2017', '15.0'),
])

# List of preferred VC toolset version based on MSVS
# Order is not relevant for this dictionary.
MSVC_TOOLSET_VERSION = {
    '2022': 'VC143',
    '2019': 'VC142',
    '2017': 'VC141',
}

def _HostIsWindows():
  """Returns True if running on a Windows host (including under cygwin)."""
  return sys.platform in ('win32', 'cygwin')

def SetEnvironmentAndGetRuntimeDllDirs():
  """Sets up os.environ to use the depot_tools VS toolchain with gyp, and
  returns the location of the VC runtime DLLs so they can be copied into
  the output directory after gyp generation.

  Return value is [x64path, x86path, 'Arm64Unused'] or None. arm64path is
  generated separately because there are multiple folders for the arm64 VC
  runtime.
  """
  vs_runtime_dll_dirs = None
  depot_tools_win_toolchain = \
      bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')))
  # When running on a non-Windows host, only do this if the SDK has explicitly
  # been downloaded before (in which case json_data_file will exist).
  if ((_HostIsWindows() or os.path.exists(json_data_file))
      and depot_tools_win_toolchain):
    if ShouldUpdateToolchain():
      if len(sys.argv) > 1 and sys.argv[1] == 'update':
        update_result = Update()
      else:
        update_result = Update(no_download=True)
      if update_result != 0:
        raise Exception('Failed to update, error code %d.' % update_result)
    with open(json_data_file, 'r') as tempf:
      toolchain_data = json.load(tempf)

    toolchain = toolchain_data['path']
    version = toolchain_data['version']
    win_sdk = toolchain_data.get('win_sdk')
    wdk = toolchain_data['wdk']
    # TODO(scottmg): The order unfortunately matters in these. They should be
    # split into separate keys for x64/x86/arm64. (See CopyDlls call below).
    # http://crbug.com/345992
    vs_runtime_dll_dirs = toolchain_data['runtime_dirs']
    # The number of runtime_dirs in the toolchain_data was two (x64/x86) but
    # changed to three (x64/x86/arm64) and this code needs to handle both
    # possibilities, which can change independently from this code.
    if len(vs_runtime_dll_dirs) == 2:
      vs_runtime_dll_dirs.append('Arm64Unused')

    os.environ['GYP_MSVS_OVERRIDE_PATH'] = toolchain

    os.environ['WINDOWSSDKDIR'] = win_sdk
    os.environ['WDK_DIR'] = wdk
    # Include the VS runtime in the PATH in case it's not machine-installed.
    runtime_path = os.path.pathsep.join(vs_runtime_dll_dirs)
    os.environ['PATH'] = runtime_path + os.path.pathsep + os.environ['PATH']
  elif sys.platform == 'win32' and not depot_tools_win_toolchain:
    if not 'GYP_MSVS_OVERRIDE_PATH' in os.environ:
      os.environ['GYP_MSVS_OVERRIDE_PATH'] = DetectVisualStudioPath()

    # When using an installed toolchain these files aren't needed in the output
    # directory in order to run binaries locally, but they are needed in order
    # to create isolates or the mini_installer. Copying them to the output
    # directory ensures that they are available when needed.
    bitness = platform.architecture()[0]
    # When running 64-bit python the x64 DLLs will be in System32
    # ARM64 binaries will not be available in the system directories because we
    # don't build on ARM64 machines.
    x64_path = 'System32' if bitness == '64bit' else 'Sysnative'
    x64_path = os.path.join(os.path.expandvars('%windir%'), x64_path)
    vs_runtime_dll_dirs = [x64_path,
                           os.path.join(os.path.expandvars('%windir%'),
                                        'SysWOW64'),
                           'Arm64Unused']

  return vs_runtime_dll_dirs


def _RegistryGetValueUsingWinReg(key, value):
  """Use the _winreg module to obtain the value of a registry key.

  Args:
    key: The registry key.
    value: The particular registry value to read.
  Return:
    contents of the registry key's value, or None on failure.  Throws
    ImportError if _winreg is unavailable.
  """
  import _winreg
  try:
    root, subkey = key.split('\\', 1)
    assert root == 'HKLM'  # Only need HKLM for now.
    with _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, subkey) as hkey:
      return _winreg.QueryValueEx(hkey, value)[0]
  except WindowsError:
    return None


def _RegistryGetValue(key, value):
  try:
    return _RegistryGetValueUsingWinReg(key, value)
  except ImportError:
    raise Exception('The python library _winreg not found.')


def GetVisualStudioVersion():
  """Return best available version of Visual Studio.
  """
  supported_versions = list(MSVS_VERSIONS.keys())

  # VS installed in depot_tools for Googlers
  if bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1'))):
    return supported_versions[0]

  # VS installed in system for external developers
  supported_versions_str = ', '.join('{} ({})'.format(v,k)
      for k,v in MSVS_VERSIONS.items())
  available_versions = []
  for version in supported_versions:
    # Checking vs%s_install environment variables.
    # For example, vs2019_install could have the value
    # "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community".
    # Only vs2017_install, vs2019_install and vs2022_install are supported.
    path = os.environ.get('vs%s_install' % version)
    if path and os.path.exists(path):
      available_versions.append(version)
      break
    # Detecting VS under possible paths.
    if version >= '2022':
      program_files_path_variable = '%ProgramFiles%'
    else:
      program_files_path_variable = '%ProgramFiles(x86)%'
    path = os.path.expandvars(program_files_path_variable +
                              '/Microsoft Visual Studio/%s' % version)
    if path and any(
        os.path.exists(os.path.join(path, edition))
        for edition in ('Enterprise', 'Professional', 'Community', 'Preview',
                        'BuildTools')):
      available_versions.append(version)
      break

  if not available_versions:
    raise Exception('No supported Visual Studio can be found.'
                    ' Supported versions are: %s.' % supported_versions_str)
  return available_versions[0]


def DetectVisualStudioPath():
  """Return path to the installed Visual Studio.
  """

  # Note that this code is used from
  # build/toolchain/win/setup_toolchain.py as well.
  version_as_year = GetVisualStudioVersion()

  # The VC++ >=2017 install location needs to be located using COM instead of
  # the registry. For details see:
  # https://blogs.msdn.microsoft.com/heaths/2016/09/15/changes-to-visual-studio-15-setup/
  # For now we use a hardcoded default with an environment variable override.
  if version_as_year >= '2022':
    program_files_path_variable = '%ProgramFiles%'
  else:
    program_files_path_variable = '%ProgramFiles(x86)%'
  for path in (os.environ.get('vs%s_install' % version_as_year),
               os.path.expandvars(program_files_path_variable +
                                  '/Microsoft Visual Studio/%s/Enterprise' %
                                  version_as_year),
               os.path.expandvars(program_files_path_variable +
                                  '/Microsoft Visual Studio/%s/Professional' %
                                  version_as_year),
               os.path.expandvars(program_files_path_variable +
                                  '/Microsoft Visual Studio/%s/Community' %
                                  version_as_year),
               os.path.expandvars(program_files_path_variable +
                                  '/Microsoft Visual Studio/%s/Preview' %
                                  version_as_year),
               os.path.expandvars(program_files_path_variable +
                                  '/Microsoft Visual Studio/%s/BuildTools' %
                                  version_as_year)):
    if path and os.path.exists(path):
      return path

  raise Exception('Visual Studio Version %s not found.' % version_as_year)


def _CopyRuntimeImpl(target, source, verbose=True):
  """Copy |source| to |target| if it doesn't already exist or if it needs to be
  updated (comparing last modified time as an approximate float match as for
  some reason the values tend to differ by ~1e-07 despite being copies of the
  same file... https://crbug.com/603603).
  """
  if (os.path.isdir(os.path.dirname(target)) and
      (not os.path.isfile(target) or
       abs(os.stat(target).st_mtime - os.stat(source).st_mtime) >= 0.01)):
    if verbose:
      print('Copying %s to %s...' % (source, target))
    if os.path.exists(target):
      # Make the file writable so that we can delete it now, and keep it
      # readable.
      os.chmod(target, stat.S_IWRITE | stat.S_IREAD)
      os.unlink(target)
    shutil.copy2(source, target)
    # Make the file writable so that we can overwrite or delete it later,
    # keep it readable.
    os.chmod(target, stat.S_IWRITE | stat.S_IREAD)

def _SortByHighestVersionNumberFirst(list_of_str_versions):
  """This sorts |list_of_str_versions| according to version number rules
  so that version "1.12" is higher than version "1.9". Does not work
  with non-numeric versions like 1.4.a8 which will be higher than
  1.4.a12. It does handle the versions being embedded in file paths.
  """
  def to_int_if_int(x):
    try:
      return int(x)
    except ValueError:
      return x

  def to_number_sequence(x):
    part_sequence = re.split(r'[\\/\.]', x)
    return [to_int_if_int(x) for x in part_sequence]

  list_of_str_versions.sort(key=to_number_sequence, reverse=True)


def _CopyUCRTRuntime(target_dir, source_dir, target_cpu, suffix):
  """Copy both the msvcp and vccorlib runtime DLLs, only if the target doesn't
  exist, but the target directory does exist."""
  if target_cpu == 'arm64':
    # Windows ARM64 VCRuntime is located at {toolchain_root}/VC/Redist/MSVC/
    # {x.y.z}/[debug_nonredist/]arm64/Microsoft.VC14x.CRT/.
    # Select VC toolset directory based on Visual Studio version
    vc_redist_root = FindVCRedistRoot()
    if suffix.startswith('.'):
      vc_toolset_dir = 'Microsoft.{}.CRT' \
         .format(MSVC_TOOLSET_VERSION[GetVisualStudioVersion()])
      source_dir = os.path.join(vc_redist_root,
                                'arm64', vc_toolset_dir)
    else:
      vc_toolset_dir = 'Microsoft.{}.DebugCRT' \
         .format(MSVC_TOOLSET_VERSION[GetVisualStudioVersion()])
      source_dir = os.path.join(vc_redist_root, 'debug_nonredist',
                                'arm64', vc_toolset_dir)
  file_parts = ('msvcp140', 'vccorlib140', 'vcruntime140')
  if target_cpu == 'x64' and GetVisualStudioVersion() != '2017':
    file_parts = file_parts + ('vcruntime140_1', )
  for file_part in file_parts:
    dll = file_part + suffix
    target = os.path.join(target_dir, dll)
    source = os.path.join(source_dir, dll)
    _CopyRuntimeImpl(target, source)
  # Copy the UCRT files from the Windows SDK. This location includes the
  # api-ms-win-crt-*.dll files that are not found in the Windows directory.
  # These files are needed for component builds. If WINDOWSSDKDIR is not set
  # use the default SDK path. This will be the case when
  # DEPOT_TOOLS_WIN_TOOLCHAIN=0 and vcvarsall.bat has not been run.
  win_sdk_dir = os.path.normpath(
      os.environ.get('WINDOWSSDKDIR',
                     os.path.expandvars('%ProgramFiles(x86)%'
                                        '\\Windows Kits\\10')))
  # ARM64 doesn't have a redist for the ucrt DLLs because they are always
  # present in the OS.
  if target_cpu != 'arm64':
    # Starting with the 10.0.17763 SDK the ucrt files are in a version-named
    # directory - this handles both cases.
    redist_dir = os.path.join(win_sdk_dir, 'Redist')
    version_dirs = glob.glob(os.path.join(redist_dir, '10.*'))
    if len(version_dirs) > 0:
      _SortByHighestVersionNumberFirst(version_dirs)
      redist_dir = version_dirs[0]
    ucrt_dll_dirs = os.path.join(redist_dir, 'ucrt', 'DLLs', target_cpu)
    ucrt_files = glob.glob(os.path.join(ucrt_dll_dirs, 'api-ms-win-*.dll'))
    assert len(ucrt_files) > 0
    for ucrt_src_file in ucrt_files:
      file_part = os.path.basename(ucrt_src_file)
      ucrt_dst_file = os.path.join(target_dir, file_part)
      _CopyRuntimeImpl(ucrt_dst_file, ucrt_src_file, False)
  # We must copy ucrtbase.dll for x64/x86, and ucrtbased.dll for all CPU types.
  if target_cpu != 'arm64' or not suffix.startswith('.'):
    if not suffix.startswith('.'):
      # ucrtbased.dll is located at {win_sdk_dir}/bin/{a.b.c.d}/{target_cpu}/
      # ucrt/.
      sdk_bin_root = os.path.join(win_sdk_dir, 'bin')
      sdk_bin_sub_dirs = glob.glob(os.path.join(sdk_bin_root, '10.*'))
      # Select the most recent SDK if there are multiple versions installed.
      _SortByHighestVersionNumberFirst(sdk_bin_sub_dirs)
      for directory in sdk_bin_sub_dirs:
        sdk_redist_root_version = os.path.join(sdk_bin_root, directory)
        if not os.path.isdir(sdk_redist_root_version):
          continue
        source_dir = os.path.join(sdk_redist_root_version, target_cpu, 'ucrt')
        break
    _CopyRuntimeImpl(os.path.join(target_dir, 'ucrtbase' + suffix),
                     os.path.join(source_dir, 'ucrtbase' + suffix))


def FindVCComponentRoot(component):
  """Find the most recent Tools or Redist or other directory in an MSVC install.
  Typical results are {toolchain_root}/VC/{component}/MSVC/{x.y.z}. The {x.y.z}
  version number part changes frequently so the highest version number found is
  used.
  """

  SetEnvironmentAndGetRuntimeDllDirs()
  assert ('GYP_MSVS_OVERRIDE_PATH' in os.environ)
  vc_component_msvc_root = os.path.join(os.environ['GYP_MSVS_OVERRIDE_PATH'],
      'VC', component, 'MSVC')
  vc_component_msvc_contents = glob.glob(
      os.path.join(vc_component_msvc_root, '14.*'))
  # Select the most recent toolchain if there are several.
  _SortByHighestVersionNumberFirst(vc_component_msvc_contents)
  for directory in vc_component_msvc_contents:
    if os.path.isdir(directory):
      return directory
  raise Exception('Unable to find the VC %s directory.' % component)


def FindVCRedistRoot():
  """In >=VS2017, Redist binaries are located in
  {toolchain_root}/VC/Redist/MSVC/{x.y.z}/{target_cpu}/.

  This returns the '{toolchain_root}/VC/Redist/MSVC/{x.y.z}/' path.
  """
  return FindVCComponentRoot('Redist')


def _CopyRuntime(target_dir, source_dir, target_cpu, debug):
  """Copy the VS runtime DLLs, only if the target doesn't exist, but the target
  directory does exist. Handles VS 2015, 2017 and 2019."""
  suffix = 'd.dll' if debug else '.dll'
  # VS 2015, 2017 and 2019 use the same CRT DLLs.
  _CopyUCRTRuntime(target_dir, source_dir, target_cpu, suffix)


def CopyDlls(target_dir, configuration, target_cpu):
  """Copy the VS runtime DLLs into the requested directory as needed.

  configuration is one of 'Debug' or 'Release'.
  target_cpu is one of 'x86', 'x64' or 'arm64'.

  The debug configuration gets both the debug and release DLLs; the
  release config only the latter.
  """
  vs_runtime_dll_dirs = SetEnvironmentAndGetRuntimeDllDirs()
  if not vs_runtime_dll_dirs:
    return

  x64_runtime, x86_runtime, arm64_runtime = vs_runtime_dll_dirs
  if target_cpu == 'x64':
    runtime_dir = x64_runtime
  elif target_cpu == 'x86':
    runtime_dir = x86_runtime
  elif target_cpu == 'arm64':
    runtime_dir = arm64_runtime
  else:
    raise Exception('Unknown target_cpu: ' + target_cpu)
  _CopyRuntime(target_dir, runtime_dir, target_cpu, debug=False)
  if configuration == 'Debug':
    _CopyRuntime(target_dir, runtime_dir, target_cpu, debug=True)
  _CopyDebugger(target_dir, target_cpu)


def _CopyDebugger(target_dir, target_cpu):
  """Copy dbghelp.dll and dbgcore.dll into the requested directory as needed.

  target_cpu is one of 'x86', 'x64' or 'arm64'.

  dbghelp.dll is used when Chrome needs to symbolize stacks. Copying this file
  from the SDK directory avoids using the system copy of dbghelp.dll which then
  ensures compatibility with recent debug information formats, such as VS
  2017 /debug:fastlink PDBs.

  dbgcore.dll is needed when using some functions from dbghelp.dll (like
  MinidumpWriteDump).
  """
  win_sdk_dir = SetEnvironmentAndGetSDKDir()
  if not win_sdk_dir:
    return

  # List of debug files that should be copied, the first element of the tuple is
  # the name of the file and the second indicates if it's optional.
  debug_files = [('dbghelp.dll', False), ('dbgcore.dll', True)]
  # The UCRT is not a redistributable component on arm64.
  if target_cpu != 'arm64':
    debug_files.extend([('api-ms-win-downlevel-kernel32-l2-1-0.dll', False),
                        ('api-ms-win-eventing-provider-l1-1-0.dll', False)])
  for debug_file, is_optional in debug_files:
    full_path = os.path.join(win_sdk_dir, 'Debuggers', target_cpu, debug_file)
    if not os.path.exists(full_path):
      if is_optional:
        continue
      else:
        raise Exception('%s not found in "%s"\r\nYou must install '
                        'Windows 10 SDK version 10.0.20348.0 including the '
                        '"Debugging Tools for Windows" feature.' %
                        (debug_file, full_path))
    target_path = os.path.join(target_dir, debug_file)
    _CopyRuntimeImpl(target_path, full_path)


def _GetDesiredVsToolchainHashes():
  """Load a list of SHA1s corresponding to the toolchains that we want installed
  to build with."""
  # Third parties that do not have access to the canonical toolchain can map
  # canonical toolchain version to their own toolchain versions.
  toolchain_hash_mapping_key = 'GYP_MSVS_HASH_%s' % TOOLCHAIN_HASH
  return [os.environ.get(toolchain_hash_mapping_key, TOOLCHAIN_HASH)]


def ShouldUpdateToolchain():
  """Check if the toolchain should be upgraded."""
  if not os.path.exists(json_data_file):
    return True
  with open(json_data_file, 'r') as tempf:
    toolchain_data = json.load(tempf)
  version = toolchain_data['version']
  env_version = GetVisualStudioVersion()
  # If there's a mismatch between the version set in the environment and the one
  # in the json file then the toolchain should be updated.
  return version != env_version


def Update(force=False, no_download=False):
  """Requests an update of the toolchain to the specific hashes we have at
  this revision. The update outputs a .json of the various configuration
  information required to pass to gyp which we use in |GetToolchainDir()|.
  If no_download is true then the toolchain will be configured if present but
  will not be downloaded.
  """
  if force != False and force != '--force':
    print('Unknown parameter "%s"' % force, file=sys.stderr)
    return 1
  if force == '--force' or os.path.exists(json_data_file):
    force = True

  depot_tools_win_toolchain = \
      bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')))
  if (_HostIsWindows() or force) and depot_tools_win_toolchain:
    import find_depot_tools
    depot_tools_path = find_depot_tools.add_depot_tools_to_path()

    # On Linux, the file system is usually case-sensitive while the Windows
    # SDK only works on case-insensitive file systems.  If it doesn't already
    # exist, set up a ciopfs fuse mount to put the SDK in a case-insensitive
    # part of the file system.
    toolchain_dir = os.path.join(depot_tools_path, 'win_toolchain', 'vs_files')
    # For testing this block, unmount existing mounts with
    # fusermount -u third_party/depot_tools/win_toolchain/vs_files
    if sys.platform.startswith('linux') and not os.path.ismount(toolchain_dir):
      ciopfs = shutil.which('ciopfs')
      if not ciopfs:
        # ciopfs not found in PATH; try the one downloaded from the DEPS hook.
        ciopfs = os.path.join(script_dir, 'ciopfs')
      if not os.path.isdir(toolchain_dir):
        os.mkdir(toolchain_dir)
      if not os.path.isdir(toolchain_dir + '.ciopfs'):
        os.mkdir(toolchain_dir + '.ciopfs')
      # Without use_ino, clang's #pragma once and Wnonportable-include-path
      # both don't work right, see https://llvm.org/PR34931
      # use_ino doesn't slow down builds, so it seems there's no drawback to
      # just using it always.
      subprocess.check_call([
          ciopfs, '-o', 'use_ino', toolchain_dir + '.ciopfs', toolchain_dir])

    get_toolchain_args = [
        sys.executable,
        os.path.join(depot_tools_path,
                    'win_toolchain',
                    'get_toolchain_if_necessary.py'),
        '--output-json', json_data_file,
      ] + _GetDesiredVsToolchainHashes()
    if force:
      get_toolchain_args.append('--force')
    if no_download:
      get_toolchain_args.append('--no-download')
    subprocess.check_call(get_toolchain_args)

  return 0


def NormalizePath(path):
  while path.endswith('\\'):
    path = path[:-1]
  return path


def SetEnvironmentAndGetSDKDir():
  """Gets location information about the current sdk (must have been
  previously updated by 'update'). This is used for the GN build."""
  SetEnvironmentAndGetRuntimeDllDirs()

  # If WINDOWSSDKDIR is not set, search the default SDK path and set it.
  if not 'WINDOWSSDKDIR' in os.environ:
    default_sdk_path = os.path.expandvars('%ProgramFiles(x86)%'
                                          '\\Windows Kits\\10')
    if os.path.isdir(default_sdk_path):
      os.environ['WINDOWSSDKDIR'] = default_sdk_path

  return NormalizePath(os.environ['WINDOWSSDKDIR'])


def GetToolchainDir():
  """Gets location information about the current toolchain (must have been
  previously updated by 'update'). This is used for the GN build."""
  runtime_dll_dirs = SetEnvironmentAndGetRuntimeDllDirs()
  win_sdk_dir = SetEnvironmentAndGetSDKDir()

  print('''vs_path = %s
sdk_path = %s
vs_version = %s
wdk_dir = %s
runtime_dirs = %s
''' % (ToGNString(NormalizePath(os.environ['GYP_MSVS_OVERRIDE_PATH'])),
       ToGNString(win_sdk_dir), ToGNString(GetVisualStudioVersion()),
       ToGNString(NormalizePath(os.environ.get('WDK_DIR', ''))),
       ToGNString(os.path.pathsep.join(runtime_dll_dirs or ['None']))))


def main():
  commands = {
      'update': Update,
      'get_toolchain_dir': GetToolchainDir,
      'copy_dlls': CopyDlls,
  }
  if len(sys.argv) < 2 or sys.argv[1] not in commands:
    print('Expected one of: %s' % ', '.join(commands), file=sys.stderr)
    return 1
  return commands[sys.argv[1]](*sys.argv[2:])


if __name__ == '__main__':
  sys.exit(main())
