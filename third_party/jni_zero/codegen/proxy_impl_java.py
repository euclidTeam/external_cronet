# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import java_types
import common


def _implicit_array_class_param(native, type_resolver):
  return_type = native.return_type
  class_name = return_type.to_array_element_type().to_java(type_resolver)
  ret = class_name + '.class'
  if native.params:
    ret = ', ' + ret
  return ret


def Generate(jni_obj, *, gen_jni_class, script_name,
             per_file_natives=False, skip_prefix_java=False):
  proxy_class = java_types.JavaClass(
      f'{jni_obj.java_class.full_name_with_slashes}Jni')
  visibility = 'public ' if jni_obj.proxy_visibility == 'public' else ''
  interface_name = jni_obj.proxy_interface.name_with_dots
  gen_jni = gen_jni_class.name
  type_resolver = java_types.TypeResolver(proxy_class)
  type_resolver.imports = jni_obj.GetClassesToBeImported()

  sb = []
  sb.append(f"""\
//
// This file was generated by {script_name}
//
package {jni_obj.java_class.class_without_prefix.package_with_dots};

""")

  import_classes = {
      'org.jni_zero.CheckDiscard', 'org.jni_zero.JniStaticTestMocker',
      'org.jni_zero.NativeLibraryLoadedStatus'
  }
  if not per_file_natives:
    import_classes.add(gen_jni_class.full_name_with_dots)

  for c in type_resolver.imports:
    if (skip_prefix_java):
      c = c.class_without_prefix
    if c.is_nested:
      # We will refer to all nested classes by OuterClass.InnerClass. We do this
      # to reduce risk of naming collisions.
      c = c.get_outer_class()
    import_classes.add(c.full_name_with_dots)
  for c in sorted(import_classes):
    sb.append(f'import {c};\n')

  if not per_file_natives:
    sb.append('\n@CheckDiscard("crbug.com/993421")')
  sb.append(f"""
{visibility}class {proxy_class.name} implements {interface_name} {{
  private static {interface_name} testInstance;

  public static final JniStaticTestMocker<{interface_name}> TEST_HOOKS =
      new JniStaticTestMocker<{interface_name}>() {{
    @Override
    public void setInstanceForTesting({interface_name} instance) {{
""")
  if not per_file_natives:
    sb.append(f"""      if (!{gen_jni}.TESTING_ENABLED) {{
        throw new RuntimeException(
            "Tried to set a JNI mock when mocks aren't enabled!");
      }}
""")

  sb.append("""      testInstance = instance;
    }
  };
""")

  for native in jni_obj.proxy_natives:
    call_params = native.params.to_call_str()
    if native.needs_implicit_array_element_class_param:
      call_params += _implicit_array_class_param(native, type_resolver)
    sig_params = native.params.to_java_declaration(type_resolver)
    return_type_str = native.return_type.to_java(type_resolver)
    return_prefix = ''
    if not native.return_type.is_void():
      return_prefix = f'return ({return_type_str}) '

    if per_file_natives:
      method_fqn = f'native{common.capitalize(native.name)}'
    else:
      method_fqn = f'{gen_jni}.{native.proxy_name}'

    sb.append(f"""
  @Override
  public {return_type_str} {native.name}({sig_params}) {{
""")
    if native.first_param_cpp_type:
      sb.append(f"""\
    assert {native.params[0].name} != 0;
""")
    sb.append(f"""\
    {return_prefix}{method_fqn}({call_params});
  }}
""")

    if per_file_natives:
      proxy_sig_params = native.proxy_params.to_java_declaration()
      proxy_return_type = native.proxy_return_type.to_java()
      sb.append(
          f'  private static native {proxy_return_type} {method_fqn}({proxy_sig_params});\n'
      )

  sb.append(f"""
  public static {interface_name} get() {{""")
  if not per_file_natives:
    sb.append(f"""
    if ({gen_jni}.TESTING_ENABLED) {{
      if (testInstance != null) {{
        return testInstance;
      }}
      if ({gen_jni}.REQUIRE_MOCK) {{
        throw new UnsupportedOperationException(
            "No mock found for the native implementation of {interface_name}. "
            + "The current configuration requires implementations be mocked.");
      }}
    }}""")
  sb.append(f"""
    NativeLibraryLoadedStatus.checkLoaded();
    return new {proxy_class.name}();
  }}
}}
""")
  return ''.join(sb)
