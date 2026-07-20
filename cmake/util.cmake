# symlink for Config/ or Content/ directory
function(crowy_link_or_copy_directory DIR_NAME)
    set(SRC "${CMAKE_SOURCE_DIR}/${DIR_NAME}")
    set(DST "${CMAKE_BINARY_DIR}/bin/${DIR_NAME}")

    if(EXISTS "${DST}")
        return()
    endif()

    if(WIN32)
        file(TO_NATIVE_PATH "${DST}" DST_NATIVE)
        file(TO_NATIVE_PATH "${SRC}" SRC_NATIVE)
        execute_process(
            COMMAND cmd /c mklink /J "${DST_NATIVE}" "${SRC_NATIVE}"
            RESULT_VARIABLE LINK_RESULT
        )
    else()
        file(CREATE_LINK "${SRC}" "${DST}"
            RESULT LINK_RESULT
            SYMBOLIC
        )
    endif()

    # fallback: copy directory
    if(NOT LINK_RESULT EQUAL 0)
        message(STATUS "[${DIR_NAME}] Symlink failed, falling back to copy...")
        file(COPY "${SRC}" DESTINATION "${CMAKE_BINARY_DIR}/bin")
    endif()
endfunction()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
crowy_link_or_copy_directory(Content)
crowy_link_or_copy_directory(Config)

function(crowy_declare_module NAME)
    add_library(Crowy${NAME} STATIC)

    file(GLOB_RECURSE PUBLIC_SOURCES
        "Public/*.hpp"
    )
    file(GLOB_RECURSE PRIVATE_SOURCES
        "Private/*.hpp"
        "Private/*.cpp"
    )

    target_sources(Crowy${NAME}
    PUBLIC
        ${PUBLIC_SOURCES}
    PRIVATE
        ${PRIVATE_SOURCES}
    )

    target_include_directories(Crowy${NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
    )

    target_link_libraries(Crowy${NAME}
    PUBLIC
        CrowyProjectInterface
    )

    add_library(Crowy::${NAME} ALIAS Crowy${NAME})
endfunction()

function(crowy_declare_private_interface NAME)
    add_library(Crowy${NAME}Private INTERFACE)

    target_include_directories(Crowy${NAME}Private
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
    )

    target_link_libraries(Crowy${NAME}Private
    INTERFACE
        Crowy::${NAME}
    )

    add_library(Crowy::${NAME}::Private ALIAS Crowy${NAME}Private)
endfunction()

function(crowy_declare_interface NAME)
    cmake_parse_arguments(ARG "" "DIRECTORY" "DEPENDS" ${ARGN})

    add_library(Crowy${NAME} INTERFACE)

    file(GLOB INTERFACE_SOURCES
        "${ARG_DIRECTORY}/*.hpp"
    )

    target_sources(Crowy${NAME}
    INTERFACE
        ${INTERFACE_SOURCES}
    )

    target_include_directories(Crowy${NAME}
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_DIRECTORY}"
    )

    target_link_libraries(Crowy${NAME}
    INTERFACE
        CrowyProjectInterface
        ${ARG_DEPENDS}
    )

    add_library(Crowy::${NAME} ALIAS Crowy${NAME})
endfunction()

function(crowy_declare_test NAME)
    cmake_parse_arguments(ARG "" "DIRECTORY" "LABELS;DEPENDS" ${ARGN})

    if(NOT ARG_DIRECTORY)
        set(ARG_DIRECTORY ".")
    endif()

    add_executable(Crowy${NAME}Test)

    file(GLOB TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_DIRECTORY}/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_DIRECTORY}/*.cpp"
    )

    target_sources(Crowy${NAME}Test
    PRIVATE
        ${TEST_SOURCES}
    )

    target_link_libraries(Crowy${NAME}Test
    PUBLIC
        CrowyProjectInterface
    PRIVATE
        GTest::gtest_main
        GTest::gmock_main
        ${ARG_DEPENDS}
    )

    add_test(
        NAME Crowy${NAME}Test
        COMMAND $<TARGET_FILE:Crowy${NAME}Test>
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_tests_properties(Crowy${NAME}Test
    PROPERTIES
        LABELS "${ARG_LABELS}"
    )
endfunction()
