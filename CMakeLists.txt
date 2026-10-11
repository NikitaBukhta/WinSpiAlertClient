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
	net/MultipartBuilder.cpp
	net/MultipartBuilder.h
	model/Image.cpp
	model/Image.h
	model/IImageCompressor.h
	model/IImageCompressorFactory.h
	model/WicImageCompressor.cpp
	model/WicImageCompressor.h
	model/WicImageCompressorFactory.cpp
	model/WicImageCompressorFactory.h
	model/ImageSizePredictor.cpp
	model/ImageSizePredictor.h
	api/ApiError.cpp
	api/ApiError.h
	api/ApiResponse.h
	api/ISpialertApi.h
	api/SpialertApi.cpp
	api/SpialertApi.h
	api/dto/StatusDto.h
	utils/TimeParser.cpp
	utils/TimeParser.h
	utils/StringUtils.cpp
	utils/StringUtils.h
	utils/UuidGenerator.cpp
	utils/UuidGenerator.h
	utils/FileUtils.cpp
	utils/FileUtils.h
	utils/ComScope.cpp
	utils/ComScope.h
	utils/UrlParser.cpp
	utils/UrlParser.h
	utils/SaxHandler.h
	api/dto/EventObjectDto.h
	api/dto/EventRequestDto.h
	api/dto/EventsListDto.h
	api/dto/ServerErrorDto.h
	app/IRetryWaiter.h
	app/SpialertService.cpp
	app/SpialertService.h
	cli/ClientConfig.h
	cli/ClientConfigLoader.cpp
	cli/ClientConfigLoader.h
	cli/CommandLine.h
	cli/CommandLineOptions.h
	cli/CommandLineParser.cpp
	cli/CommandLineParser.h
	cli/CommandRunner.cpp
	cli/CommandRunner.h
	cli/ConsoleRetryWaiter.cpp
	cli/ConsoleRetryWaiter.h
)

target_include_directories(client_core PUBLIC ${rapidjson_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client_core PUBLIC winhttp rpcrt4 windowscodecs ole32)
# NOHELP: winuser.h defines WinHelp macros like HELP_COMMAND, they clash with CommandEnum values
target_compile_definitions(client_core PUBLIC NOHELP)

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

	# --------------------------- #
	# -------- UNIT TESTS ------- #
	# --------------------------- #

	add_executable(
		client_tests
		tests/api/ApiErrorTests.cpp
		tests/api/ApiResponseTests.cpp
		tests/api/SpialertApiTests.cpp
		tests/app/SpialertServiceTests.cpp
		tests/cli/ClientConfigLoaderTests.cpp
		tests/cli/CommandLineParserTests.cpp
		tests/api/dto/EventObjectDtoTests.cpp
		tests/api/dto/EventRequestDtoTests.cpp
		tests/api/dto/EventsListDtoTests.cpp
		tests/api/dto/ServerErrorDtoTests.cpp
		tests/api/dto/StatusDtoTests.cpp
		tests/net/WinHttpClientTests.cpp
		tests/model/ImageTests.cpp
		tests/model/WicImageCompressorTests.cpp
		tests/model/ImageSizePredictorTests.cpp
		tests/helpers/ImageCodec.h
		tests/net/MultipartBuilderTests.cpp
		tests/utils/FileUtilsTests.cpp
		tests/utils/StringUtilsTests.cpp
		tests/utils/TimeParserTests.cpp
		tests/utils/UrlParserTests.cpp
		tests/utils/UuidGeneratorTests.cpp
	)
	target_link_libraries(client_tests PRIVATE client_core GTest::gtest_main GTest::gmock)

	include(GoogleTest)
	gtest_discover_tests(client_tests)

	# --------------------------- #
	# ---- INTEGRATION TESTS ---- #
	# --------------------------- #

	add_executable(
		client_integration_tests
		tests/helpers/ClientProcess.cpp
		tests/helpers/ClientProcess.h
		tests/helpers/HandlePtr.h
		tests/helpers/MockServerProcess.cpp
		tests/helpers/MockServerProcess.h
		tests/integration/main.cpp
		tests/integration/MockServerEnvironment.cpp
		tests/integration/MockServerEnvironment.h
		tests/integration/ClientCliIntegrationTests.cpp
		tests/integration/SpialertApiIntegrationTests.cpp
		tests/integration/SpialertServiceIntegrationTests.cpp
		tests/integration/WinHttpClientIntegrationTests.cpp
	)

	target_link_libraries(client_integration_tests PRIVATE client_core GTest::gtest ws2_32)
	target_compile_definitions(
		client_integration_tests
		PRIVATE MOCK_SERVER_PYTHON="${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe"
				MOCK_SERVER_SCRIPT="${CMAKE_SOURCE_DIR}/scripts/mock_server.py"
				MOCK_SERVER_LOG="${CMAKE_CURRENT_BINARY_DIR}/mock_server.log"
				TEST_IMAGES_MANIFEST="${CMAKE_SOURCE_DIR}/tests/data/images.json"
				TEST_IMAGES_DIR="${CMAKE_BINARY_DIR}/test-images"
				CLIENT_EXE="$<TARGET_FILE:client>"
	)

	# the CLI tests run client.exe itself
	add_dependencies(client_integration_tests client)

	add_test(NAME client_integration_tests COMMAND client_integration_tests)
	set_tests_properties(client_integration_tests PROPERTIES LABELS integration)
endif()
