cmake_minimum_required(VERSION 3.16)
project(SlintWrapper)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# Add the Slint library path
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmake")

# Find Slint package
find_package(Slint REQUIRED)

# Find jsoncpp package
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${JSONCPP_INCLUDE_DIRS})

# Create the executable
add_executable(slint-wrapper src/main.cpp)

# Link against Slint interpreter library and jsoncpp
target_link_libraries(slint-wrapper PRIVATE Slint::Slint ${JSONCPP_LIBRARIES})
target_link_directories(slint-wrapper PRIVATE ${JSONCPP_LIBRARY_DIRS})

# Set rpath for finding the shared library at runtime
set_target_properties(slint-wrapper PROPERTIES
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Enable all warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(slint-wrapper PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(slint-wrapper PRIVATE /W4)
endif()

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Slint library: ${Slint_DIR}")