#  Copyright (C) 2023 The Android Open Source Project
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import collections
import logging as log
import re
from typing import Dict, Set, Callable

import gn_target
import utils
from android.tools.gn2bp.gn_target import GnTarget


class GnParser:
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

    def __init__(self, builtin_deps):
        self.builtin_deps: Dict[str, Callable] = builtin_deps
        self.all_targets: Dict[str, gn_target.GnTarget] = {}
        self.java_sources: Dict[str, Set[str]] = collections.defaultdict(set)
        self.aidl_local_include_dirs: Set[str] = set()
        self.java_actions: Dict[str, Set[str]] = collections.defaultdict(set)

    def get_target(self, gn_target_name: str) -> GnTarget | None:
        """
        Returns a GnTarget object from the fully qualified GN target name.

        :param gn_target_name: GN target label
        :return: GnTarget if there exists a target with such name.
        """
        if gn_target_name in self.all_targets:
            self.all_targets[gn_target_name].finalize()
            return self.all_targets[gn_target_name]
        return None

    def _is_target_already_visited(self, gn_target_name: str, variant: utils.Variant) -> bool:
        return gn_target_name in self.all_targets and \
            variant in self.all_targets[gn_target_name].get_variants()

    def parse_gn_desc(self, gn_desc: Dict[str,], gn_target_name: str,
                      java_group_name: str | None = None,
                      is_test_target: bool = False) -> GnTarget | None:
        """Parses a gn desc tree and resolves all target dependencies.

        It bubbles up variables from source_set dependencies as described in the
        class-level comments.
        """
        target_name = utils.label_without_toolchain(gn_target_name)
        desc = gn_desc[gn_target_name]
        type_ = desc['type']
        variant = utils.get_variant(desc['toolchain'])

        # This check must happen before doing any changes to the target_name as the
        # builtin_deps dictionary is not aware of the presence of __TESTING modules.
        if target_name in self.builtin_deps:
            # return early, no need to parse any further as the module is a builtin.
            return None

        if utils.is_java_group(type_, target_name):
            java_group_name = target_name

        if is_test_target:
            target_name += utils.TESTING_SUFFIX

        if self._is_target_already_visited(target_name, variant):
            return self.all_targets[target_name]

        target = None
        if utils.is_proto_target(type_, gn_desc.get("script", "")):
            target = gn_target.ProtoGnTarget(target_name)
        elif target.type == 'source_set':
            target.arch[arch].sources.update(desc.get('sources', []))
        elif target.is_linker_unit_type():
            target.arch[arch].sources.update(desc.get('sources', []))
        elif (desc.get("script", "") in utils.JAVA_BANNED_SCRIPTS
              or self._is_java_group(target.type, target.name)):
            # java_group identifies the group target generated by the android_library
            # or java_library template. A java_group must not be added as a
            # dependency, but sources are collected.
            log.debug('Found java target %s', target.name)
            if target.type == "action":
                # Convert java actions into java_group and keep the inputs for collection.
                target.inputs.update(desc.get('inputs', []))
            target.type = 'java_group'
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
        for gn_dep_name in desc.get('deps', []):
            dep = self.parse_gn_desc(gn_desc, gn_dep_name, java_group_name, is_test_target)
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
            elif dep.type == 'java_group':
                # Explicitly break dependency chain when a java_group is added.
                # Java sources are collected and eventually compiled as one large
                # java_library.
                pass

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

            # Collect java sources. Java sources are kept inside the __compile_java target.
            # This target can be used for both host and target compilation; only add
            # the sources if they are destined for the target (i.e. they are a
            # dependency of the __dex target)
            # Note: this skips prebuilt java dependencies. These will have to be
            # added manually when building the jar.
            if target.name.endswith('__dex'):
                if dep.name.endswith('__compile_java'):
                    log.debug('Adding java sources for %s', dep.name)
                    java_srcs = [src for src in dep.inputs if utils.is_java_source(src)]
                    if not is_test_target:
                        # TODO(aymanm): Fix collecting sources for testing modules for java.
                        # Don't collect java source files for test targets.
                        # We only need a specific set of java sources which are hardcoded in gen_android_bp
                        self.java_sources[java_group_name].update(java_srcs)
            if dep.type in ["action"] and target.type == "java_group":
                # GN uses an action to compile aidl files. However, this is not needed in soong
                # as soong can directly have .aidl files in srcs. So adding .aidl files to the java_sources.
                # TODO: Find a better way/place to do this.
                if not is_test_target:
                    if '_aidl' in dep.name:
                        self.java_sources[java_group_name].update(dep.arch[arch].sources)
                        self.aidl_local_include_dirs.update(
                            utils.extract_includes_from_aidl_args(dep.arch[arch].args))
                    else:
                        # TODO(aymanm): Fix collecting actions for testing modules for java.
                        # Don't collect java actions for test targets.
                        self.java_actions[java_group_name].add(dep.name)
        return target
