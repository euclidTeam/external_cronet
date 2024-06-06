set(LIBCXX_CXX_ABI libcxxabi CACHE STRING "")

set(LIBCXXABI_ENABLE_SHARED OFF CACHE BOOL "")
set(LIBCXX_ENABLE_STATIC_ABI_LIBRARY ON CACHE BOOL "")

set(LIBCXX_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBCXXABI_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBUNWIND_USE_COMPILER_RT ON CACHE BOOL "")

# Without this flag, 'long double' (which is 80 bit on x86 mingw, but
# 64 bit in MSVC) isn't handled correctly in printf.
set(LIBCXX_EXTRA_SITE_DEFINES "__USE_MINGW_ANSI_STDIO=1" CACHE STRING "")
