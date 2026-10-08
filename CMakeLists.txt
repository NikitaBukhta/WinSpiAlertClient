cmake_minimum_required(VERSION 3.14)
project(LowResourceClient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
	string(REPLACE "/O2" "/Ox" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	add_compile_options(/utf-8 $<$<CONFIG:Release>:/Gw$<SEMICOLON>/Gy$<SEMICOLON>/Oi>)
	add_link_options($<$<CONFIG:Release>:/OPT:REF$<SEMICOLON>/OPT:ICF>) # optimization: remove unused code;
endif()

# Can be replaced with vcpkg in future / create own sdk
include(FetchContent)
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	rapidjson
	GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
	GIT_TAG master
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rapidjson)

add_executable(client main.cpp)

target_include_directories(client PRIVATE ${rapidjson_SOURCE_DIR}/include)
target_link_libraries(client PRIVATE winhttp rpcrt4)
