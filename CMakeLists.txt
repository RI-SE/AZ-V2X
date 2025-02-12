cmake_minimum_required(VERSION 3.14)

# Project name and version
project(AZ-V2X VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetchExternalDeps.cmake")

# Find OpenSSL first (required by Vanetza)
find_package(OpenSSL REQUIRED)
# Find vanetza package
find_package(Vanetza REQUIRED)
# Find qpid-proton using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(QPID REQUIRED IMPORTED_TARGET
    libqpid-proton
    libqpid-proton-cpp
)

# Collect all source files automatically
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Add executable
add_executable(${PROJECT_NAME} ${SRC_FILES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vanetza::common
    Vanetza::net
    Vanetza::btp
    Vanetza::geonet
    Vanetza::dcc
    Vanetza::facilities
    PkgConfig::QPID
    spdlog)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${spdlog_SOURCE_DIR}
    ${VANETZA_INCLUDE_DIRS}
)

# Add any additional configurations or targets here