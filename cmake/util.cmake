set(ASSET_SRC "${CMAKE_SOURCE_DIR}/asset")
set(ASSET_DST "${CMAKE_BINARY_DIR}/bin/asset")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(NOT EXISTS "${ASSET_DST}")
    # try to link asset folder
    if(WIN32)
        execute_process(
            COMMAND cmd /c mklink /J "${ASSET_DST}" "${ASSET_SRC}"
            RESULT_VARIABLE LINK_RESULT
        )
    else()
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${ASSET_SRC}" "${ASSET_DST}"
            RESULT_VARIABLE LINK_RESULT
        )
    endif()

    # fallback: copy asset folder
    if(NOT LINK_RESULT EQUAL 0)
        message(STATUS "Symlink failed, falling back to copy...")
        file(COPY "${ASSET_SRC}" DESTINATION "${CMAKE_BINARY_DIR}/bin")
    endif()
endif()

function(crowy_declare_module NAME)
    add_library(Crowy${NAME} STATIC)

    file(GLOB PUBLIC_SOURCES
        "Public/*.hpp"
    )
    file(GLOB PRIVATE_SOURCES
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

    add_library(Crowy::${NAME} ALIAS Crowy${NAME})
endfunction()

function(crowy_declare_private_interface NAME)
    add_library(Crowy${NAME}_Private INTERFACE)

    target_include_directories(Crowy${NAME}_Private INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
    )

    target_link_libraries(Crowy${NAME}_Private INTERFACE Crowy::${NAME})
endfunction()

function(crowy_declare_interface NAME)
    cmake_parse_arguments(ARG "" "DIRECTORY" "" ${ARGN})

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
    PRIVATE
        GTest::gtest_main
        GTest::gmock_main
        ${ARG_DEPENDS}
    )

    add_test(
        NAME Crowy${NAME}-Test
        COMMAND $<TARGET_FILE:Crowy${NAME}Test>
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_tests_properties(Crowy${NAME}-Test
    PROPERTIES
        LABELS "${ARG_LABELS}"
    )
endfunction()

function(crowy_rhi_macro NAME)
    if(RENDER_BACKEND STREQUAL "Metal")
        target_compile_definitions(Crowy${NAME}
        PRIVATE
            CROWY_METALRHI
        )
    elseif(RENDER_BACKEND STREQUAL "D3D11")
        target_compile_definitions(Crowy${NAME}
        PRIVATE
            CROWY_D3DRHI
            CROWY_D3D11RHI
        )
    elseif(RENDER_BACKEND STREQUAL "D3D12")
        target_compile_definitions(Crowy${NAME}
        PRIVATE
            CROWY_D3DRHI
            CROWY_D3D12RHI
        )
    endif()
endfunction()