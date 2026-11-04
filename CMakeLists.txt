cmake_minimum_required(VERSION 3.21)

project(DAEDRA VERSION 0.1.0 LANGUAGES CXX)

# Bump on every build handed to a tester; shown in the log and in the zip name.
set(DAEDRA_REVISION 22)

option(DAEDRA_BUILD_PLUGIN "Build the SKSE plugin (needs CommonLibSSE-NG in extern/)" ON)
option(DAEDRA_BUILD_TESTS "Build the rule-engine unit tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)

# ---- pure C++ rule engine (no game types) ---------------------------------
add_library(daedra_core STATIC
	core/src/Facts.cpp
	core/src/Rules.cpp
	core/src/Engine.cpp)
target_include_directories(daedra_core PUBLIC core/include)
target_link_libraries(daedra_core PUBLIC nlohmann_json::nlohmann_json)
if(MSVC)
	target_compile_options(daedra_core PRIVATE /W4 /permissive- /utf-8)
endif()

# ---- unit tests ------------------------------------------------------------
if(DAEDRA_BUILD_TESTS)
	enable_testing()
	find_package(doctest CONFIG REQUIRED)
	add_executable(daedra_tests tests/test_engine.cpp)
	target_link_libraries(daedra_tests PRIVATE daedra_core doctest::doctest)
	if(MSVC)
		target_compile_options(daedra_tests PRIVATE /W4 /permissive- /utf-8)
	endif()
	add_test(NAME daedra_tests COMMAND daedra_tests)
endif()

# ---- SKSE plugin -----------------------------------------------------------
if(DAEDRA_BUILD_PLUGIN)
	set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(extern/CommonLibSSE-NG)
	include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

	# Plugin info is declared in plugin/src/main.cpp (the helper cannot set the post-1.6.629 struct flag).
	add_library(DAEDRA SHARED
		plugin/src/main.cpp
		plugin/src/Config.cpp
		plugin/src/Log.cpp
		plugin/src/Adapter.cpp
		plugin/src/Dump.cpp
		plugin/src/MenuHook.cpp
		plugin/src/Api.cpp
		plugin/src/Apply.cpp)
	target_link_libraries(DAEDRA PUBLIC CommonLibSSE::CommonLibSSE)
	target_compile_definitions(DAEDRA PRIVATE __CMAKE_COMMONLIBSSE_PLUGIN=1 DAEDRA_REVISION=${DAEDRA_REVISION})
	target_include_directories(DAEDRA PRIVATE plugin/src api)
	target_link_libraries(DAEDRA PRIVATE daedra_core)
	target_compile_definitions(DAEDRA PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
	if(MSVC)
		target_compile_options(DAEDRA PRIVATE /utf-8 /permissive- /Zc:preprocessor /EHsc "/FI${CMAKE_CURRENT_SOURCE_DIR}/plugin/src/ForceInclude.h")
	endif()
endif()
