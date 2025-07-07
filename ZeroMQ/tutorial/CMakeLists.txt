cmake_minimum_required(VERSION 3.15)
project(RouterDealerExample)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Find cppzmq
find_path(CPPZMQ_INCLUDE_DIR 
    NAMES zmq.hpp
    PATHS ${ZMQ_INCLUDE_DIRS}
          /usr/local/include
          /usr/include
          /opt/homebrew/include
          /opt/homebrew/Cellar/cppzmq/*/include
)

# Find Threads package
find_package(Threads REQUIRED)

# Create executables
add_executable(router_dealer_official router_dealer_official.cpp)
add_executable(router_dealer_basic router_dealer_basic.cpp)

# Include directories for both executables
target_include_directories(router_dealer_official PRIVATE 
    ${ZMQ_INCLUDE_DIRS}
    ${CPPZMQ_INCLUDE_DIR}
)

target_include_directories(router_dealer_basic PRIVATE 
    ${ZMQ_INCLUDE_DIRS}
    ${CPPZMQ_INCLUDE_DIR}
)

# Link libraries for both executables
target_link_libraries(router_dealer_official 
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

target_link_libraries(router_dealer_basic 
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

# Compiler flags for both executables
target_compile_options(router_dealer_official PRIVATE 
    ${ZMQ_CFLAGS_OTHER}
    -Wall -Wextra -O2
)

target_compile_options(router_dealer_basic PRIVATE 
    ${ZMQ_CFLAGS_OTHER}
    -Wall -Wextra -O2
)

# Link directories for both executables
target_link_directories(router_dealer_official PRIVATE ${ZMQ_LIBRARY_DIRS})
target_link_directories(router_dealer_basic PRIVATE ${ZMQ_LIBRARY_DIRS})

# Print some information
message(STATUS "ZMQ_INCLUDE_DIRS: ${ZMQ_INCLUDE_DIRS}")
message(STATUS "ZMQ_LIBRARIES: ${ZMQ_LIBRARIES}")
message(STATUS "CPPZMQ_INCLUDE_DIR: ${CPPZMQ_INCLUDE_DIR}")
