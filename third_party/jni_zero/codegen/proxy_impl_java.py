# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import java_types


def _implicit_array_class_param(native, type_resolver):
  return_type = native.return_type
  class_name = return_type.to_array_element_type().to_java(type_resolver)
  ret = class_name + '.class'
  if native.params:
    ret = ', ' + ret
  return ret


def Generate(jni_obj, *, gen_jni_class, script_name):
  proxy_class = java_types.JavaClass(
      f'{jni_obj.java_class.full_name_with_slashes}Jni')
  visibility = 'public ' if jni_obj.proxy_visibility == 'public' else ''
  interface_name = jni_obj.proxy_interface.name_with_dots
  gen_jni = gen_jni_class.name
  type_resolver = java_types.TypeResolver(proxy_class)
  type_resolver.imports = list(jni_obj.type_resolver.imports)

  sb = []
  sb.append(f"""\
//
// This file was generated by {script_name}
//
package {jni_obj.java_class.class_without_prefix.package_with_dots};

import org.jni_zero.CheckDiscard;
import org.jni_zero.JniStaticTestMocker;
import org.jni_zero.NativeLibraryLoadedStatus;
import {gen_jni_class.full_name_with_dots};
""")

  # Copy over all imports (some will be unused, but oh well).
  for c in type_resolver.imports:
    sb.append(f'import {c.full_name_with_dots};\n')

  sb.append(f"""
@CheckDiscard("crbug.com/993421")
{visibility}class {proxy_class.name} implements {interface_name} {{
  private static {interface_name} testInstance;

  public static final JniStaticTestMocker<{interface_name}> TEST_HOOKS =
      new JniStaticTestMocker<{interface_name}>() {{
    @Override
    public void setInstanceForTesting({interface_name} instance) {{
      if (!{gen_jni}.TESTING_ENABLED) {{
        throw new RuntimeException(
            "Tried to set a JNI mock when mocks aren't enabled!");
      }}
      testInstance = instance;
    }}
  }};
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

    sb.append(f"""
  @Override
  public {return_type_str} {native.name}({sig_params}) {{
""")
    if native.first_param_cpp_type:
      sb.append(f"""\
    assert {native.params[0].name} != 0;
""")
    sb.append(f"""\
    {return_prefix}{gen_jni}.{native.proxy_name}({call_params});
  }}
""")

  sb.append(f"""
  public static {interface_name} get() {{
    if ({gen_jni}.TESTING_ENABLED) {{
      if (testInstance != null) {{
        return testInstance;
      }}
      if ({gen_jni}.REQUIRE_MOCK) {{
        throw new UnsupportedOperationException(
            "No mock found for the native implementation of {interface_name}. "
            + "The current configuration requires implementations be mocked.");
      }}
    }}
    NativeLibraryLoadedStatus.checkLoaded();
    return new {proxy_class.name}();
  }}
}}
""")
  return ''.join(sb)
