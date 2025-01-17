cmake_minimum_required(VERSION 3.10)
project(TestNetIOMP CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find emp-tool
find_package(emp-tool REQUIRED)

# Add your executable
add_executable(test_netiomp test_netiomp.cpp)

# Link emp-tool and pthread
target_link_libraries(test_netiomp PRIVATE emp-tool pthread)