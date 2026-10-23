# Standalone build of the maths unit tests; needs no OpenCPN or wxWidgets.
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.12)
project(cbf_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
  add_compile_options(/W4 /utf-8)
else ()
  add_compile_options(-Wall -Wextra)
endif ()

add_executable(test_cbf_math test_cbf_math.cpp ../src/cbf_math.cpp)
target_include_directories(test_cbf_math PRIVATE ../src)

enable_testing()
add_test(NAME cbf_math COMMAND test_cbf_math)
