cmake_minimum_required(VERSION 3.28)

project(Crowy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# option(CROWY_BUILD_TOOLS "Build tools" ON)
option(CROWY_ENABLE_TEST "Enable Tests" ON)
option(CROWY_STATIC_RHI "Use STATIC RHI model" OFF)

list(APPEND CMAKE_PREFIX_PATH ${CMAK_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(cmake/config.cmake)

add_subdirectory(Core)
add_subdirectory(Engine)

if(CROWY_ENABLE_TEST)
    # add_subdirectory(Test)
endif()
