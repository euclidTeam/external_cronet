#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generates a single BUILD.gn file with build targets generated using the
# manifest files in the SDK.

import json
import logging
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

from common import DIR_SRC_ROOT, SDK_ROOT, get_host_os

# Inserted at the top of the generated BUILD.gn file.
_GENERATED_PREAMBLE = """# DO NOT EDIT! This file was generated by
# //build/fuchsia/gen_build_def.py.
# Any changes made to this file will be discarded.

import("//third_party/fuchsia-gn-sdk/src/fidl_library.gni")
import("//third_party/fuchsia-gn-sdk/src/fuchsia_sdk_pkg.gni")

"""


def ReformatTargetName(dep_name):
  """"Substitutes characters in |dep_name| which are not valid in GN target
  names (e.g. dots become hyphens)."""
  return dep_name


def FormatGNTarget(fields):
  """Returns a GN target definition as a string.

  |fields|: The GN fields to include in the target body.
            'target_name' and 'type' are mandatory."""

  output = '%s("%s") {\n' % (fields['type'], fields['target_name'])
  del fields['target_name']
  del fields['type']

  # Ensure that fields with no ordering requirement are sorted.
  for field in ['sources', 'public_deps']:
    if field in fields:
      fields[field].sort()

  for key, val in fields.items():
    if isinstance(val, str):
      val_serialized = '\"%s\"' % val
    elif isinstance(val, list):
      # Serialize a list of strings in the prettiest possible manner.
      if len(val) == 0:
        val_serialized = '[]'
      elif len(val) == 1:
        val_serialized = '[ \"%s\" ]' % val[0]
      else:
        val_serialized = '[\n    ' + ',\n    '.join(['\"%s\"' % x
                                                     for x in val]) + '\n  ]'
    else:
      raise Exception('Could not serialize %r' % val)

    output += '  %s = %s\n' % (key, val_serialized)
  output += '}'

  return output


def MetaRootRelativePaths(sdk_relative_paths, meta_root):
  return [os.path.relpath(path, meta_root) for path in sdk_relative_paths]


def ConvertCommonFields(json):
  """Extracts fields from JSON manifest data which are used across all
  target types. Note that FIDL packages do their own processing."""

  meta_root = json['root']

  converted = {'target_name': ReformatTargetName(json['name'])}

  if 'deps' in json:
    converted['public_deps'] = MetaRootRelativePaths(json['deps'],
                                                     os.path.dirname(meta_root))

  # FIDL bindings dependencies are relative to the "fidl" sub-directory.
  if 'fidl_binding_deps' in json:
    for entry in json['fidl_binding_deps']:
      converted['public_deps'] += MetaRootRelativePaths([
          'fidl/' + dep + ':' + os.path.basename(dep) + '_' +
          entry['binding_type'] for dep in entry['deps']
      ], meta_root)

  return converted


def ConvertFidlLibrary(json):
  """Converts a fidl_library manifest entry to a GN target.

  Arguments:
    json: The parsed manifest JSON.
  Returns:
    The GN target definition, represented as a string."""

  meta_root = json['root']

  converted = ConvertCommonFields(json)
  converted['type'] = 'fidl_library'
  converted['sources'] = MetaRootRelativePaths(json['sources'], meta_root)
  converted['library_name'] = json['name']

  return converted


def ConvertCcPrebuiltLibrary(json):
  """Converts a cc_prebuilt_library manifest entry to a GN target.

  Arguments:
    json: The parsed manifest JSON.
  Returns:
    The GN target definition, represented as a string."""

  meta_root = json['root']

  converted = ConvertCommonFields(json)
  converted['type'] = 'fuchsia_sdk_pkg'

  converted['sources'] = MetaRootRelativePaths(json['headers'], meta_root)

  converted['include_dirs'] = MetaRootRelativePaths([json['include_dir']],
                                                    meta_root)

  if json['format'] == 'shared':
    converted['shared_libs'] = [json['name']]
  else:
    converted['static_libs'] = [json['name']]

  return converted


def ConvertCcSourceLibrary(json):
  """Converts a cc_source_library manifest entry to a GN target.

  Arguments:
    json: The parsed manifest JSON.
  Returns:
    The GN target definition, represented as a string."""

  meta_root = json['root']

  converted = ConvertCommonFields(json)
  converted['type'] = 'fuchsia_sdk_pkg'

  # Headers and source file paths can be scattered across "sources", "headers",
  # and "files". Merge them together into one source list.
  converted['sources'] = MetaRootRelativePaths(json['sources'], meta_root)
  if 'headers' in json:
    converted['sources'] += MetaRootRelativePaths(json['headers'], meta_root)
  if 'files' in json:
    converted['sources'] += MetaRootRelativePaths(json['files'], meta_root)
  converted['sources'] = list(set(converted['sources']))

  converted['include_dirs'] = MetaRootRelativePaths([json['include_dir']],
                                                    meta_root)

  return converted


def ConvertLoadableModule(json):
  """Converts a loadable module manifest entry to GN targets.

  Arguments:
    json: The parsed manifest JSON.
  Returns:
    A list of GN target definitions."""

  name = json['name']
  if name != 'vulkan_layers':
    raise RuntimeError('Unsupported loadable_module: %s' % name)

  # Copy resources and binaries
  resources = json['resources']

  binaries = json['binaries']

  def _filename_no_ext(name):
    return os.path.splitext(os.path.basename(name))[0]

  # Pair each json resource with its corresponding binary. Each such pair
  # is a "layer". We only need to check one arch because each arch has the
  # same list of binaries.
  arch = next(iter(binaries))
  binary_names = binaries[arch]
  local_pkg = json['root']
  vulkan_targets = []

  for res in resources:
    layer_name = _filename_no_ext(res)

    # Filter binaries for a matching name.
    filtered = [n for n in binary_names if _filename_no_ext(n) == layer_name]

    if not filtered:
      # If the binary could not be found then do not generate a
      # target for this layer. The missing targets will cause a
      # mismatch with the "golden" outputs.
      continue

    # Replace hardcoded arch in the found binary filename.
    binary = filtered[0].replace('/' + arch + '/', "/${target_cpu}/")

    target = {}
    target['name'] = layer_name
    target['config'] = os.path.relpath(res, start=local_pkg)
    target['binary'] = os.path.relpath(binary, start=local_pkg)

    vulkan_targets.append(target)

  converted = []
  all_target = {}
  all_target['target_name'] = 'all'
  all_target['type'] = 'group'
  all_target['data_deps'] = []
  for target in vulkan_targets:
    config_target = {}
    config_target['target_name'] = target['name'] + '_config'
    config_target['type'] = 'copy'
    config_target['sources'] = [target['config']]
    config_target['outputs'] = ['${root_gen_dir}/' + target['config']]
    converted.append(config_target)
    lib_target = {}
    lib_target['target_name'] = target['name'] + '_lib'
    lib_target['type'] = 'copy'
    lib_target['sources'] = [target['binary']]
    lib_target['outputs'] = ['${root_out_dir}/lib/{{source_file_part}}']
    converted.append(lib_target)
    group_target = {}
    group_target['target_name'] = target['name']
    group_target['type'] = 'group'
    group_target['data_deps'] = [
        ':' + target['name'] + '_config', ':' + target['name'] + '_lib'
    ]
    converted.append(group_target)
    all_target['data_deps'].append(':' + target['name'])
  converted.append(all_target)
  return converted


def ConvertNoOp(json):
  """Null implementation of a conversion function. No output is generated."""

  return None


"""Maps manifest types to conversion functions."""
_CONVERSION_FUNCTION_MAP = {
    'fidl_library': ConvertFidlLibrary,
    'cc_source_library': ConvertCcSourceLibrary,
    'cc_prebuilt_library': ConvertCcPrebuiltLibrary,
    'loadable_module': ConvertLoadableModule,

    # No need to build targets for these types yet.
    'companion_host_tool': ConvertNoOp,
    'component_manifest': ConvertNoOp,
    'config': ConvertNoOp,
    'dart_library': ConvertNoOp,
    'data': ConvertNoOp,
    'device_profile': ConvertNoOp,
    'documentation': ConvertNoOp,
    'ffx_tool': ConvertNoOp,
    'host_tool': ConvertNoOp,
    'image': ConvertNoOp,
    'sysroot': ConvertNoOp,
}


def ConvertMeta(meta_path):
  parsed = json.load(open(meta_path))
  if 'type' not in parsed:
    return

  convert_function = _CONVERSION_FUNCTION_MAP.get(parsed['type'])
  if convert_function is None:
    logging.warning('Unexpected SDK artifact type %s in %s.' %
                    (parsed['type'], meta_path))
    return

  converted = convert_function(parsed)
  if not converted:
    return
  output_path = os.path.join(os.path.dirname(meta_path), 'BUILD.gn')
  if os.path.exists(output_path):
    os.unlink(output_path)
  with open(output_path, 'w') as buildfile:
    buildfile.write(_GENERATED_PREAMBLE)

    # Loadable modules have multiple targets
    if convert_function != ConvertLoadableModule:
      buildfile.write(FormatGNTarget(converted) + '\n\n')
    else:
      for target in converted:
        buildfile.write(FormatGNTarget(target) + '\n\n')


def ProcessSdkManifest():
  toplevel_meta = json.load(
      open(os.path.join(SDK_ROOT, 'meta', 'manifest.json')))

  for part in toplevel_meta['parts']:
    meta_path = os.path.join(SDK_ROOT, part['meta'])
    ConvertMeta(meta_path)


def main():

  # Exit if there's no Fuchsia support for this platform.
  try:
    get_host_os()
  except:
    logging.warning('Fuchsia SDK is not supported on this platform.')
    return 0

  # TODO(crbug/1432399): Remove this when links to these files inside the sdk
  # directory have been redirected.
  build_path = os.path.join(SDK_ROOT, 'build')
  os.makedirs(build_path, exist_ok=True)
  for gn_file in ['component.gni', 'package.gni']:
    open(os.path.join(build_path, gn_file),
         "w").write("""# DO NOT EDIT! This file was generated by
# //build/fuchsia/gen_build_def.py.
# Any changes made to this file will be discarded.

import("//third_party/fuchsia-gn-sdk/src/{}")
      """.format(gn_file))

  ProcessSdkManifest()


if __name__ == '__main__':
  sys.exit(main())
