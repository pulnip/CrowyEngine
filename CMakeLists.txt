cmake_minimum_required(VERSION 3.28)

project(Crowy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CROWY_ENABLE_TEST "Enable Tests" ON)
# orthogonal to the build type: a benchmark build is a Debug or a Release
# build that also measures. off means not one counter survives the compile.
option(CROWY_BENCHMARK "Enable frame instrumentation" OFF)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(config)

add_subdirectory(Engine)
