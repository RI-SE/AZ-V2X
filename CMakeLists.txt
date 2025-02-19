cmake_minimum_required(VERSION 3.14)

# Project name and version
project(AZ-V2X VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include external dependencies
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetchExternalDeps.cmake")

# Enable testing
enable_testing()

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(Vanetza REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QPID REQUIRED IMPORTED_TARGET
    libqpid-proton
    libqpid-proton-cpp
)

# Add GTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Get all source files except main.cpp
file(GLOB_RECURSE LIB_SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
list(REMOVE_ITEM LIB_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Create library target
add_library(${PROJECT_NAME}_lib ${LIB_SRC_FILES})

# Get Vanetza include directories and dependencies
get_target_property(VANETZA_SECURITY_INCLUDE_DIRS Vanetza::security INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(VANETZA_ASN1_INCLUDE_DIRS Vanetza::asn1 INTERFACE_INCLUDE_DIRECTORIES)

# Set include directories for library
target_include_directories(${PROJECT_NAME}_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${spdlog_SOURCE_DIR}
    ${VANETZA_SECURITY_INCLUDE_DIRS}
    ${VANETZA_ASN1_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Link libraries to the library target
target_link_libraries(${PROJECT_NAME}_lib PUBLIC 
    Vanetza::common
    Vanetza::net
    Vanetza::btp
    Vanetza::geonet
    Vanetza::dcc
    Vanetza::facilities
    Vanetza::asn1
    PkgConfig::QPID
    Vanetza::security
    Boost::program_options
    spdlog
)

# Add main executable
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Create test directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Add test executable
add_executable(${PROJECT_NAME}_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/denm_message_test.cpp
)

target_link_libraries(${PROJECT_NAME}_test PRIVATE
    ${PROJECT_NAME}_lib
    GTest::gtest
    GTest::gtest_main
)

# Enable testing
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)

# Install targets
install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_lib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)