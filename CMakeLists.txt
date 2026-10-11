cmake_minimum_required(VERSION 3.20) # CMAKE_CXX_STANDARD 23
project(WinSpiAlertClient CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
	string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	add_compile_options(/utf-8 $<$<CONFIG:Release>:/Gw$<SEMICOLON>/Oi>)
	add_link_options($<$<CONFIG:Release>:/OPT:REF$<SEMICOLON>/OPT:ICF>) # remove unused and fold identical code
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Can be replaced with vcpkg in future / create own sdk
include(FetchContent)
FetchContent_Declare(
	rapidjson
	GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
	GIT_TAG master
	GIT_SHALLOW TRUE
	SOURCE_SUBDIR _no_cmake # header-only: skip add_subdirectory of rapidjson's own CMakeLists
)
FetchContent_MakeAvailable(rapidjson)

add_library(
	client_core STATIC
	net/IHttpClient.h
	net/WinHttpClient.cpp
	net/WinHttpClient.h
	api/ISpialertAPI.h
	api/SpialertApi.cpp
	api/SpialertApi.h
	api/dto/StatusDto.h
	utils/TimeParser.cpp
	utils/TimeParser.h
	utils/StringUtils.cpp
	utils/StringUtils.h
	utils/RapidjsonHelper.h
	api/dto/EventObjectDto.h
	api/dto/EventsListDto.h
)

target_include_directories(client_core PUBLIC ${rapidjson_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client_core PUBLIC winhttp rpcrt4)

add_executable(client main.cpp)
target_link_libraries(client PRIVATE client_core)

option(WINSPIALERT_BUILD_TESTS "Build unit tests (GoogleTest)" ON)
if(WINSPIALERT_BUILD_TESTS)
	enable_testing()

	FetchContent_Declare(
		googletest
		GIT_REPOSITORY https://github.com/google/googletest.git
		GIT_TAG v1.17.0
		GIT_SHALLOW TRUE
	)
	# MSVC: same CRT (/MD) as the rest of the project
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	add_executable(
		client_tests
		tests/api/ApiResponseTests.cpp tests/api/SpialertApiTests.cpp tests/api/dto/EventObjectDtoTests.cpp
		tests/api/dto/EventsListDtoTests.cpp tests/api/dto/StatusDtoTests.cpp tests/net/WinHttpClientTests.cpp
		tests/utils/StringUtilsTests.cpp tests/utils/TimeParserTests.cpp
	)
	target_link_libraries(client_tests PRIVATE client_core GTest::gtest_main GTest::gmock)

	include(GoogleTest)
	gtest_discover_tests(client_tests)
endif()
