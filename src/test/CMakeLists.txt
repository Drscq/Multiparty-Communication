cmake_minimum_required(VERSION 3.5)
project(TestNetIOMP LANGUAGES CXX)

# Use C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# ----------------------------------------------------------------------------
# 1) Locate emp-tool
#    We assume emp-tool is in ../emp-tool relative to your "src/test" folder.
#    If it's not exactly two levels up, adjust the path below.
# ----------------------------------------------------------------------------
set(EMP_TOOL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../emp-tool")

message(STATUS "EMP_TOOL_ROOT_DIR = ${EMP_TOOL_ROOT_DIR}")

# Include path so the compiler can find #include <emp-tool/emp-tool.h>
include_directories("${EMP_TOOL_ROOT_DIR}/include")

# Library path for linking libemp-tool.* in emp-tool/build/
link_directories("${EMP_TOOL_ROOT_DIR}/build")

# ----------------------------------------------------------------------------
# 2) Add the Source File(s)
# ----------------------------------------------------------------------------
add_executable(test_netiomap test_netiomp.cpp)

# ----------------------------------------------------------------------------
# 3) Link Libraries
# ----------------------------------------------------------------------------
# Link with emp-tool library and pthread
target_link_libraries(test_netiomap
    PRIVATE
    emp-tool
    pthread
)

# Optional: Add some warning flags
target_compile_options(test_netiomap PRIVATE -Wall -O2)
