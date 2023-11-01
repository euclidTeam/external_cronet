# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# A collection of utilities for extracting build rule information from GN
# projects.

import copy
import json
import logging as log
import os
import re
import collections

LINKER_UNIT_TYPES = ('executable', 'shared_library', 'static_library', 'source_set')
JAVA_BANNED_SCRIPTS = [
    "//build/android/gyp/turbine.py",
    "//build/android/gyp/compile_java.py",
    "//build/android/gyp/filter_zip.py",
    "//build/android/gyp/dex.py",
    "//build/android/gyp/write_build_config.py",
    "//build/android/gyp/create_r_java.py",
    "//build/android/gyp/ijar.py",
    "//build/android/gyp/create_r_java.py",
    "//build/android/gyp/bytecode_processor.py",
    "//build/android/gyp/prepare_resources.py",
    "//build/android/gyp/aar.py",
    "//build/android/gyp/zip.py",
]
# This is a list of java files that should not be collected
# as they don't exist right now downstream (eg: apihelpers, cronetEngineBuilderTest).
# This is temporary solution until they are up-streamed.
JAVA_FILES_TO_IGNORE = (
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/ByteArrayCronetCallback.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/ContentTypeParametersParser.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/CronetRequestCompletionListener.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/CronetResponse.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/ImplicitFlowControlCallback.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/InMemoryTransformCronetCallback.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/JsonCronetCallback.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/RedirectHandler.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/RedirectHandlers.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/StringCronetCallback.java",
  "//components/cronet/android/api/src/org/chromium/net/apihelpers/UrlRequestCallbacks.java",
  "//components/cronet/android/test/javatests/src/org/chromium/net/CronetEngineBuilderTest.java",
  # The following tests are currently not included in the tests because they
  # depends on H2 test server.
  "//components/cronet/android/test/javatests/src/org/chromium/net/BidirectionalStreamTest.java",
  "//components/cronet/android/test/javatests/src/org/chromium/net/MockCertVerifierTest.java",
  "//components/cronet/android/test/javatests/src/org/chromium/net/NetworkErrorLoggingTest.java",
  "//components/cronet/android/test/javatests/src/org/chromium/net/PkpTest.java",
  # Api helpers does not exist downstream, hence the tests shouldn't be collected.
  "//components/cronet/android/test/javatests/src/org/chromium/net/apihelpers/ContentTypeParametersParserTest.java",
  # Netty does not exist currently in AOSP so those classes won't compile. We replace with stubs.
  "//components/cronet/android/test/src/org/chromium/net/Http2TestHandler.java",
  "//components/cronet/android/test/src/org/chromium/net/Http2TestServer.java"
)
RESPONSE_FILE = '{{response_file_name}}'
TESTING_SUFFIX = "__testing"
AIDL_INCLUDE_DIRS_REGEX = r'--includes=\[(.*)\]'

def repo_root():
  """Returns an absolute path to the repository root."""
  return os.path.join(
      os.path.realpath(os.path.dirname(__file__)), os.path.pardir)


def _clean_string(str):
  return str.replace('\\', '').replace('../../', '').replace('"', '').strip()

def _extract_includes_from_aidl_args(args):
  for arg in args:
    is_match = re.match(AIDL_INCLUDE_DIRS_REGEX, arg)
    if is_match:
      local_includes = is_match.group(1).split(",")
      return [_clean_string(local_include) for local_include in local_includes]
  return []

def contains_aidl(sources):
  return len([src for src in sources if src.endswith(".aidl")]) > 0

def label_to_path(label):
  """Turn a GN output label (e.g., //some_dir/file.cc) into a path."""
  assert label.startswith('//')
  return label[2:] or "./"

def label_without_toolchain(label):
  """Strips the toolchain from a GN label.

    Return a GN label (e.g //buildtools:protobuf(//gn/standalone/toolchain:
    gcc_like_host) without the parenthesised toolchain part.
    """
  return label.split('(')[0]


def _is_java_source(src):
  return os.path.splitext(src)[1] == '.java' and not src.startswith("//out/")


class GnParser(object):
  """A parser with some cleverness for GN json desc files

    The main goals of this parser are:
    1) Deal with the fact that other build systems don't have an equivalent
       notion to GN's source_set. Conversely to Bazel's and Soong's filegroups,
       GN source_sets expect that dependencies, cflags and other source_set
       properties propagate up to the linker unit (static_library, executable or
       shared_library). This parser simulates the same behavior: when a
       source_set is encountered, some of its variables (cflags and such) are
       copied up to the dependent targets. This is to allow gen_xxx to create
       one filegroup for each source_set and then squash all the other flags
       onto the linker unit.
    2) Detect and special-case protobuf targets, figuring out the protoc-plugin
       being used.
    """

  class Target(object):
    """Reperesents A GN target.

        Maked properties are propagated up the dependency chain when a
        source_set dependency is encountered.
        """
    class Arch():
      """Architecture-dependent properties
        """
      def __init__(self):
        self.sources = set()
        self.cflags = set()
        self.defines = set()
        self.include_dirs = set()
        self.deps = set()
        self.transitive_static_libs_deps = set()
        self.source_set_deps = set()
        self.ldflags = set()

        # These are valid only for type == 'action'
        self.inputs = set()
        self.outputs = set()
        self.args = []
        self.response_file_contents = ''

    def __init__(self, name, type):
      self.name = name  # e.g. //src/ipc:ipc

      VALID_TYPES = ('static_library', 'shared_library', 'executable', 'group',
                     'action', 'source_set', 'proto_library', 'copy', 'action_foreach')
      assert (type in VALID_TYPES)
      self.type = type
      self.testonly = False
      self.toolchain = None

      # These are valid only for type == proto_library.
      # This is typically: 'proto', 'protozero', 'ipc'.
      self.proto_plugin = None
      self.proto_paths = set()
      self.proto_exports = set()
      self.proto_in_dir = ""

      # TODO(primiano): consider whether the public section should be part of
      # bubbled-up sources.
      self.public_headers = set()  # 'public'

      # These are valid only for type == 'action'
      self.script = ''

      # These variables are propagated up when encountering a dependency
      # on a source_set target.
      self.libs = set()
      self.proto_deps = set()
      self.transitive_proto_deps = set()
      self.rtti = False

      # TODO: come up with a better way to only run this once.
      # is_finalized tracks whether finalize() was called on this target.
      self.is_finalized = False
      # 'common' is a pseudo-architecture used to store common architecture dependent properties (to
      # make handling of common vs architecture-specific arguments more consistent).
      self.arch = {'common': self.Arch()}

      # This is used to get the name/version of libcronet
      self.output_name = None
      # Local Includes used for AIDL
      self.local_aidl_includes = set()
      # Deps for JNI Registration
      self.jni_registration_java_deps = set()
      # Transitive Java Sources
      self.transitive_java_sources = set()

    # Properties to forward access to common arch.
    # TODO: delete these after the transition has been completed.
    @property
    def sources(self):
      return self.arch['common'].sources

    @sources.setter
    def sources(self, val):
      self.arch['common'].sources = val

    @property
    def inputs(self):
      return self.arch['common'].inputs

    @inputs.setter
    def inputs(self, val):
      self.arch['common'].inputs = val

    @property
    def outputs(self):
      return self.arch['common'].outputs

    @outputs.setter
    def outputs(self, val):
      self.arch['common'].outputs = val

    @property
    def args(self):
      return self.arch['common'].args

    @args.setter
    def args(self, val):
      self.arch['common'].args = val

    @property
    def response_file_contents(self):
      return self.arch['common'].response_file_contents

    @response_file_contents.setter
    def response_file_contents(self, val):
      self.arch['common'].response_file_contents = val

    @property
    def cflags(self):
      return self.arch['common'].cflags

    @property
    def defines(self):
      return self.arch['common'].defines

    @property
    def deps(self):
      return self.arch['common'].deps

    @deps.setter
    def deps(self, val):
      self.arch['common'].deps = val


    @property
    def include_dirs(self):
      return self.arch['common'].include_dirs

    @property
    def ldflags(self):
      return self.arch['common'].ldflags

    @property
    def source_set_deps(self):
      return self.arch['common'].source_set_deps

    def host_supported(self):
      return 'host' in self.arch

    def device_supported(self):
      return any([name.startswith('android') for name in self.arch.keys()])

    def is_linker_unit_type(self):
      return self.type in LINKER_UNIT_TYPES

    def __lt__(self, other):
      if isinstance(other, self.__class__):
        return self.name < other.name
      raise TypeError(
          '\'<\' not supported between instances of \'%s\' and \'%s\'' %
          (type(self).__name__, type(other).__name__))

    def __repr__(self):
      return json.dumps({
          k: (list(sorted(v)) if isinstance(v, set) else v)
          for (k, v) in self.__dict__.items()
      },
                        indent=4,
                        sort_keys=True)

    def update(self, other, arch):
      for key in ('cflags', 'defines', 'deps', 'include_dirs', 'ldflags',
                  'source_set_deps', 'proto_deps', 'transitive_proto_deps',
                  'libs', 'proto_paths'):
        getattr(self, key).update(getattr(other, key, []))

      for key_in_arch in ('cflags', 'defines', 'include_dirs', 'source_set_deps', 'ldflags'):
        getattr(self.arch[arch], key_in_arch).update(getattr(other.arch[arch], key_in_arch, []))

    def get_archs(self):
      """ Returns a dict of archs without the common arch """
      return {arch: val for arch, val in self.arch.items() if arch != 'common'}

    def _finalize_set_attribute(self, key):
      # Target contains the intersection of arch-dependent properties
      getattr(self, key).update(set.intersection(*[getattr(arch, key) for arch in
                                                   self.get_archs().values()]))

      # Deduplicate arch-dependent properties
      for arch in self.get_archs().values():
        getattr(arch, key).difference_update(getattr(self, key))

    def _finalize_non_set_attribute(self, key):
      # Only when all the arch has the same non empty value, move the value to the target common
      val = getattr(list(self.get_archs().values())[0], key)
      if val and all([val == getattr(arch, key) for arch in self.get_archs().values()]):
        setattr(self, key, copy.deepcopy(val))

    def _finalize_attribute(self, key):
      val = getattr(self, key)
      if isinstance(val, set):
        self._finalize_set_attribute(key)
      elif isinstance(val, (list, str)):
        self._finalize_non_set_attribute(key)
      else:
        raise TypeError(f'Unsupported type: {type(val)}')

    def finalize(self):
      """Move common properties out of arch-dependent subobjects to Target object.

        TODO: find a better name for this function.
        """
      if self.is_finalized:
        return
      self.is_finalized = True

      if len(self.arch) == 1:
        return

      for key in ('sources', 'cflags', 'defines', 'include_dirs', 'deps', 'source_set_deps',
                  'inputs', 'outputs', 'args', 'response_file_contents', 'ldflags'):
        self._finalize_attribute(key)

    def get_target_name(self):
      return self.name[self.name.find(":") + 1:]


  def __init__(self, builtin_deps):
    self.builtin_deps = builtin_deps
    self.all_targets = {}
    self.java_sources = collections.defaultdict(set)
    self.java_actions = collections.defaultdict(set)

  def _get_response_file_contents(self, action_desc):
    # response_file_contents are formatted as:
    # ['--flags', '--flag=true && false'] and need to be formatted as:
    # '--flags --flag=\"true && false\"'
    flags = action_desc.get('response_file_contents', [])
    formatted_flags = []
    for flag in flags:
      if '=' in flag:
        key, val = flag.split('=')
        formatted_flags.append('%s=\\"%s\\"' % (key, val))
      else:
        formatted_flags.append(flag)

    return ' '.join(formatted_flags)

  def _is_java_group(self, type_, target_name):
    # Per https://chromium.googlesource.com/chromium/src/build/+/HEAD/android/docs/java_toolchain.md
    # java target names must end in "_java".
    # TODO: There are some other possible variations we might need to support.
    return type_ == 'group' and target_name.endswith('_java')

  def _get_arch(self, toolchain):
    if toolchain == '//build/toolchain/android:android_clang_x86':
      return 'android_x86'
    elif toolchain == '//build/toolchain/android:android_clang_x64':
      return 'android_x86_64'
    elif toolchain == '//build/toolchain/android:android_clang_arm':
      return 'android_arm'
    elif toolchain == '//build/toolchain/android:android_clang_arm64':
      return 'android_arm64'
    else:
      return 'host'

  def get_target(self, gn_target_name):
    """Returns a Target object from the fully qualified GN target name.

      get_target() requires that parse_gn_desc() has already been called.
      """
    # Run this every time as parse_gn_desc can be called at any time.
    for target in self.all_targets.values():
      target.finalize()

    return self.all_targets[label_without_toolchain(gn_target_name)]

  def parse_gn_desc(self, gn_desc, gn_target_name, java_group_name=None, is_test_target=False):
    """Parses a gn desc tree and resolves all target dependencies.

        It bubbles up variables from source_set dependencies as described in the
        class-level comments.
        """
    # Use name without toolchain for targets to support targets built for
    # multiple archs.
    target_name = label_without_toolchain(gn_target_name)
    desc = gn_desc[gn_target_name]
    type_ = desc['type']
    arch = self._get_arch(desc['toolchain'])
    metadata = desc.get("metadata", {})

    if is_test_target:
      target_name += TESTING_SUFFIX

    target = self.all_targets.get(target_name)
    if target is None:
      target = GnParser.Target(target_name, type_)
      self.all_targets[target_name] = target

    if arch not in target.arch:
      target.arch[arch] = GnParser.Target.Arch()
    else:
      return target  # Target already processed.

    if 'cur_type' in metadata.keys() and metadata["cur_type"][0] == 'java_library':
      target.type = 'java_library'

    if target.name in self.builtin_deps:
      # return early, no need to parse any further as the module is a builtin.
      return target

    target.testonly = desc.get('testonly', False)

    proto_target_type, proto_desc = self.get_proto_target_type(gn_desc, gn_target_name)
    deps = desc.get("deps", {})
    if proto_target_type is not None:
      target.type = 'proto_library'
      target.proto_plugin = proto_target_type
      target.proto_paths.update(self.get_proto_paths(proto_desc))
      target.proto_exports.update(self.get_proto_exports(proto_desc))
      target.proto_in_dir = self.get_proto_in_dir(proto_desc)
      for gn_proto_deps_name in proto_desc.get('deps', []):
        dep = self.parse_gn_desc(gn_desc, gn_proto_deps_name)
        target.deps.add(dep.name)
      target.arch[arch].sources.update(proto_desc.get('sources', []))
      assert (all(x.endswith('.proto') for x in target.arch[arch].sources))
    elif target.type == 'source_set':
      target.arch[arch].sources.update(desc.get('sources', []))
    elif target.is_linker_unit_type():
      target.arch[arch].sources.update(desc.get('sources', []))
    elif target.type == 'java_library':
      target.sources.update(java_source for java_source in metadata.get("source_files", []) if not java_source.startswith("//out")
                            and java_source not in JAVA_FILES_TO_IGNORE)
      target.transitive_java_sources.update(target.sources)
      deps = desc["metadata"].get("all_deps", {})
      log.info('Found Java Target %s', target.name)
    elif desc.get("script", "") == "//build/android/gyp/aidl.py":
      target.type = "java_library"
      target.sources.update(desc.get('sources', {}))
      target.local_aidl_includes = _extract_includes_from_aidl_args(desc.get('args', ''))
    elif desc.get("script", "") == '//build/android/gyp/zip.py':
      target.type = 'java_import'
      # Ignore build_config dependencies as those are only used upstream in chromium.
      deps = set(dep for dep in deps if "__build_config" not in dep)
    elif target.type in ['action', 'action_foreach']:
      target.arch[arch].inputs.update(desc.get('inputs', []))
      target.arch[arch].sources.update(desc.get('sources', []))
      outs = [re.sub('^//out/.+?/gen/', '', x) for x in desc['outputs']]
      target.arch[arch].outputs.update(outs)
      # While the arguments might differ, an action should always use the same script for every
      # architecture. (gen_android_bp's get_action_sanitizer actually relies on this fact.
      target.script = desc['script']
      target.arch[arch].args = desc['args']
      target.arch[arch].response_file_contents = self._get_response_file_contents(desc)
      target.jni_registration_java_deps.update(metadata.get("java_deps", set()))
    elif target.type == 'copy':
      # TODO: copy rules are not currently implemented.
      pass

    # Default for 'public' is //* - all headers in 'sources' are public.
    # TODO(primiano): if a 'public' section is specified (even if empty), then
    # the rest of 'sources' is considered inaccessible by gn. Consider
    # emulating that, so that generated build files don't end up with overly
    # accessible headers.
    public_headers = [x for x in desc.get('public', []) if x != '*']
    target.public_headers.update(public_headers)

    target.arch[arch].cflags.update(desc.get('cflags', []) + desc.get('cflags_cc', []))
    target.libs.update(desc.get('libs', []))
    target.arch[arch].ldflags.update(desc.get('ldflags', []))
    target.arch[arch].defines.update(desc.get('defines', []))
    target.arch[arch].include_dirs.update(desc.get('include_dirs', []))
    target.output_name = desc.get('output_name', None)
    if "-frtti" in target.arch[arch].cflags:
      target.rtti = True

    # Recurse in dependencies.
    for gn_dep_name in (set(deps) | target.jni_registration_java_deps):
      dep = self.parse_gn_desc(gn_desc, gn_dep_name, java_group_name, is_test_target)

      if gn_dep_name in target.jni_registration_java_deps:
        # Do not add jni_registration_java_deps as part of the deps as we only need to collect the
        # java sources that comes from those dependencies.
        target.sources.update(dep.transitive_java_sources)
        continue

      if dep.type == 'proto_library':
        target.proto_deps.add(dep.name)
        target.transitive_proto_deps.add(dep.name)
        target.proto_paths.update(dep.proto_paths)
        target.transitive_proto_deps.update(dep.transitive_proto_deps)
      elif dep.type == 'group':
        target.update(dep, arch)  # Bubble up groups's cflags/ldflags etc.
      elif dep.type in ['action', 'action_foreach', 'copy']:
        if proto_target_type is None:
          target.arch[arch].deps.add(dep.name)
      elif dep.is_linker_unit_type():
        target.arch[arch].deps.add(dep.name)
      elif dep.type in ['java_library', 'java_import']:
        target.deps.add(dep.name)
        target.transitive_java_sources.update(dep.transitive_java_sources)

      if dep.type in ['static_library', 'source_set']:
        # Bubble up static_libs and source_set. Necessary, since soong does not propagate
        # static_libs up the build tree.
        # Source sets are later translated to static_libraries, so it makes sense
        # to reuse transitive_static_libs_deps.
        target.arch[arch].transitive_static_libs_deps.add(dep.name)

      if arch in dep.arch:
        target.arch[arch].transitive_static_libs_deps.update(
            dep.arch[arch].transitive_static_libs_deps)
        target.arch[arch].deps.update(target.arch[arch].transitive_static_libs_deps)
    return target

  def get_proto_exports(self, proto_desc):
    # exports in metadata will be available for source_set targets.
    metadata = proto_desc.get('metadata', {})
    return metadata.get('exports', [])

  def get_proto_paths(self, proto_desc):
    # import_dirs in metadata will be available for source_set targets.
    metadata = proto_desc.get('metadata', {})
    return metadata.get('import_dirs', [])


  def get_proto_in_dir(self, proto_desc):
    args = proto_desc.get('args')
    return re.sub('^\.\./\.\./', '', args[args.index('--proto-in-dir') + 1])

  def get_proto_target_type(self, gn_desc, gn_target_name):
    """ Checks if the target is a proto library and return the plugin.

        Returns:
            (None, None): if the target is not a proto library.
            (plugin, proto_desc) where |plugin| is 'proto' in the default (lite)
            case or 'protozero' or 'ipc' or 'descriptor'; |proto_desc| is the GN
            json desc of the target with the .proto sources (_gen target for
            non-descriptor types or the target itself for descriptor type).
        """
    parts = gn_target_name.split('(', 1)
    name = parts[0]
    toolchain = '(' + parts[1] if len(parts) > 1 else ''

    # Descriptor targets don't have a _gen target; instead we look for the
    # characteristic flag in the args of the target itself.
    desc = gn_desc.get(gn_target_name)
    if '--descriptor_set_out' in desc.get('args', []):
      return 'descriptor', desc

    # Source set proto targets have a non-empty proto_library_sources in the
    # metadata of the description.
    metadata = desc.get('metadata', {})
    if 'proto_library_sources' in metadata:
      return 'source_set', desc

    # In all other cases, we want to look at the _gen target as that has the
    # important information.
    gen_desc = gn_desc.get('%s_gen%s' % (name, toolchain))
    if gen_desc is None or gen_desc['type'] != 'action':
      return None, None
    if gen_desc['script'] != '//tools/protoc_wrapper/protoc_wrapper.py':
      return None, None
    plugin = 'proto'
    args = gen_desc.get('args', [])
    for arg in (arg for arg in args if arg.startswith('--plugin=')):
      # |arg| at this point looks like:
      #  --plugin=protoc-gen-plugin=gcc_like_host/protozero_plugin
      # or
      #  --plugin=protoc-gen-plugin=protozero_plugin
      plugin = arg.split('=')[-1].split('/')[-1].replace('_plugin', '')
    return plugin, gen_desc
