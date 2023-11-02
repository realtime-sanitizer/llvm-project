include(CompilerRTCompile)

include_directories(..)

set(RADSAN_UNITTEST_CFLAGS
  ${COMPILER_RT_UNITTEST_CFLAGS}
  ${COMPILER_RT_GTEST_CFLAGS}
  ${COMPILER_RT_GMOCK_CFLAGS}
  ${SANITIZER_TEST_CXX_CFLAGS}
  -I${COMPILER_RT_SOURCE_DIR}/lib/
  -I${COMPILER_RT_SOURCE_DIR}/include/
  -I${COMPILER_RT_SOURCE_DIR}/lib/radsan
  -I${COMPILER_RT_SOURCE_DIR}/lib/sanitizer_common/tests
  -DSANITIZER_COMMON_NO_REDEFINE_BUILTINS
  -O2
  -fsanitize=realtime)

set(RADSAN_UNITTESTS
    radsan_test.cpp
    radsan_test_interceptors.cpp
    radsan_test_main.cpp)

set(RADSAN_UNITTEST_HEADERS
    radsan_test_utilities.h)

add_custom_target(RadsanUnitTests)
set_target_properties(RadsanUnitTests PROPERTIES FOLDER "Compiler-RT Tests")

set(RADSAN_UNITTEST_LINK_FLAGS
  ${COMPILER_RT_UNITTEST_LINK_FLAGS}
  ${COMPILER_RT_UNWINDER_LINK_LIBS}
  ${SANITIZER_TEST_CXX_LIBRARIES})

if (APPLE)
  list(APPEND RADSAN_UNITTEST_LINK_FLAGS ${DARWIN_osx_LINK_FLAGS})
  list(APPEND RADSAN_UNITTEST_CFLAGS ${DARWIN_osx_CFLAGS})
  # aligned_alloc is only available in macOS 10.15 and later. This is a temporary
  # solution that we're running with until we have a full understanding of what
  # macOS versions we wish to support.
  list(APPEND RADSAN_UNITTEST_CFLAGS -mmacosx-version-min=10.15)
endif()

list(APPEND RADSAN_UNITTEST_LINK_FLAGS -fsanitize=realtime)

if(COMPILER_RT_DEFAULT_TARGET_ARCH IN_LIST RADSAN_SUPPORTED_ARCH)
  set(arch ${COMPILER_RT_DEFAULT_TARGET_ARCH})

  set(RadsanTestObjects)
  generate_compiler_rt_tests(RadsanTestObjects
    RadsanUnitTests "Radsan-${arch}-Test" ${arch}
    COMPILE_DEPS ${RADSAN_UNITTEST_HEADERS}
    SOURCES ${RADSAN_UNITTESTS} ${COMPILER_RT_GTEST_SOURCE}
    DEPS llvm_gtest radsan
    CFLAGS ${RADSAN_UNITTEST_CFLAGS} ${TEST_CFLAGS}
    LINK_FLAGS ${RADSAN_UNITTEST_LINK_FLAGS})
  set_target_properties(RadsanUnitTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()
