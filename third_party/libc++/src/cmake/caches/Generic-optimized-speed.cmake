set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "")
set(LIBCXX_TEST_PARAMS "optimization=speed" CACHE STRING "")
set(LIBCXXABI_TEST_PARAMS "${LIBCXX_TEST_PARAMS}" CACHE STRING "")
set(LIBUNWIND_TEST_PARAMS "${LIBCXX_TEST_PARAMS}" CACHE STRING "")
