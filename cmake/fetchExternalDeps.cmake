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

# Declare dependencies:
# FetchContent_Declare(websocketpp
# 	GIT_REPOSITORY     https://github.com/RI-SE/websocketpp
# 	GIT_TAG            0.8.3
# )
FetchContent_Declare(spdlog
	GIT_REPOSITORY     https://github.com/gabime/spdlog
	GIT_TAG            v1.9.2
)
# FetchContent_Declare(googletest
# 	GIT_REPOSITORY     https://github.com/google/googletest.git
# 	GIT_TAG            v1.13.0
# )
# FetchContent_Declare(nlohmann_json
# 	GIT_REPOSITORY https://github.com/nlohmann/json.git
# 	GIT_TAG v3.10.0
# )
# FetchContent_Declare(nlohmann_json_schema_validator
# 	GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
# 	GIT_TAG 2.1.0
# 	GIT_SHALLOW        OFF # pull whole git commit history
# 	OVERRIDE_FIND_PACKAGE #use the json provided by FetchContent instead of using find_package
# )

# Include dependencies as subdirectories:

# Set options for specific packages. See respective options file in cmake/xxxOptions.cmake
# include("${CMAKE_SOURCE_DIR}/cmake/websocketppOptions.cmake")

FetchContent_MakeAvailable(
	# websocketpp
	spdlog
	# googletest
)
