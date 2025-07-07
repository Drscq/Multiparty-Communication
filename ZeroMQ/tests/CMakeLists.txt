cmake_minimum_required(VERSION 3.10)
project(ZMQMPCCommunicationTest)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------- ZeroMQ C library --------
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBZMQ REQUIRED libzmq)        # ← keeps the C lib search

# -------- cppzmq header-only C++ binding --------
find_package(cppzmq REQUIRED)                       # ← NEW

# ---------- GoogleTest ----------
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_executable(test_zmq_mpc_communication
    test_zmq_mpc_communication.cpp
)

# ----- Include directories -----
target_include_directories(test_zmq_mpc_communication
    PRIVATE
        ${PC_LIBZMQ_INCLUDE_DIRS}   # libzmq headers
        /opt/homebrew/include       # cppzmq headers (contains zmq.hpp)
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# ----- Link libraries -----
target_link_libraries(test_zmq_mpc_communication
    PRIVATE
        ${PC_LIBZMQ_LIBRARIES}      # -lzmq
        gtest gtest_main pthread
)

# ----- Link directories -----
target_link_directories(test_zmq_mpc_communication
    PRIVATE
        ${PC_LIBZMQ_LIBRARY_DIRS}   # ZMQ library directories
)