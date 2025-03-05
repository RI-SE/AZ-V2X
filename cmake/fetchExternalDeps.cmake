# Dependencies
include(FetchContent)
execute_process(
	COMMAND ping www.github.com -c 1 -q
	ERROR_QUIET
	RESULT_VARIABLE NO_CONNECTION
)

if(NOT NO_CONNECTION EQUAL 0)
	set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
	message(WARNING "Fetch offline mode: requires already populated _deps")
else()
	set(FETCHCONTENT_UPDATES_DISCONNECTED OFF)
endif()

# Add spdlog
FetchContent_Declare(spdlog
	GIT_REPOSITORY     https://github.com/gabime/spdlog
	GIT_TAG            v1.9.2
)

# Add Crow
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.0+5
)

# Add nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)

# Add GTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

# Include dependencies as subdirectories:

# Set options for specific packages. See respective options file in cmake/xxxOptions.cmake
# include("${CMAKE_SOURCE_DIR}/cmake/websocketppOptions.cmake")

FetchContent_MakeAvailable(
	spdlog
	crow
	json
	googletest
)
