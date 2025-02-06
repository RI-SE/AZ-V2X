cmake_minimum_required(VERSION 3.14)

# Project name and version
project(AZ-V2X VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find vanetza package
find_package(Vanetza REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    # Add other source files here
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE vanetza)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add any additional configurations or targets here