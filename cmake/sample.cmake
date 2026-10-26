# Executable taxonomy. Every executable in the tree is one of three kinds,
# legible from its folder and its name:
#
#   Example  no suffix   windowed, shows something
#   Spike    *Spike      cross-backend semantics pinned by its picture
#   Check    *Check      headless, exits nonzero on a wrong number
#
# An executable gets a ctest entry only when its declaration names a label
# with TEST (docs/testing.md): gpu for a check, render for a spike, app for
# a host whose start and shutdown are the test. Without one it is built only.

# App and the Main<T> entry template windowed executables are built on
set(CROWY_APP_FRAMEWORK_DIR "${CMAKE_SOURCE_DIR}/Engine/RHI/Sample")

function(crowy_declare_executable NAME)
    cmake_parse_arguments(ARG "FRAMEWORK" "TEST;FRAMES" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME}
        ${ARG_SOURCES}
    )
    crowy_copy_runtime_files(${NAME})

    target_link_options(${NAME}
    PRIVATE
        ${RENDER_BACKEND_LINK_OPTIONS}
    )
    target_link_libraries(${NAME}
    PUBLIC
        CrowyProjectInterface
    PRIVATE
        ${RENDER_BACKENDS}
        Crowy::Log
        ${ARG_DEPENDS}
    )

    if(ARG_FRAMEWORK)
        target_sources(${NAME}
        PRIVATE
            "${CROWY_APP_FRAMEWORK_DIR}/AppFramework.cpp"
        )
        target_include_directories(${NAME}
        PRIVATE
            "${CROWY_APP_FRAMEWORK_DIR}"
        )
        target_link_libraries(${NAME}
        PRIVATE
            Crowy::Platform
        )
    endif()

    if(NOT ARG_TEST)
        return()
    endif()
    if(NOT ARG_TEST MATCHES "^(gpu|render|app)$")
        message(FATAL_ERROR "${NAME}: TEST is gpu, render or app, not '${ARG_TEST}'")
    endif()

    # launch and watch through Tools/smoke_run.sh|.ps1: a windowed one exits
    # by itself after FRAMES frames (the scripts' default when not given)
    if(APPLE)
        set(SMOKE_COMMAND
            "${CMAKE_SOURCE_DIR}/Tools/smoke_run.sh"
            "$<TARGET_FILE:${NAME}>"
        )
    elseif(WIN32)
        set(SMOKE_COMMAND
            powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/Tools/smoke_run.ps1"
            "$<TARGET_FILE:${NAME}>"
        )
    endif()
    if(NOT DEFINED SMOKE_COMMAND)
        return()
    endif()

    add_test(
        NAME ${NAME}
        COMMAND ${SMOKE_COMMAND}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(${NAME}
    PROPERTIES
        LABELS "${ARG_TEST}"
        # generous: the watch window is env-configurable
        TIMEOUT 300
        # a sample whose content is missing exits 77, and the scripts pass
        # it through
        SKIP_RETURN_CODE 77
    )
    if(ARG_TEST STREQUAL "gpu")
        # a check that outlives the watch window is hung, not healthy; with
        # no window to focus and no port to bind, checks run side by side
        set_property(TEST ${NAME}
            APPEND PROPERTY ENVIRONMENT "CROWY_SMOKE_MUST_EXIT=1"
        )
    else()
        # windowed apps fight over focus and ports; one at a time keeps
        # results stable
        set_tests_properties(${NAME} PROPERTIES RUN_SERIAL TRUE)
    endif()
    if(ARG_FRAMES)
        set_property(TEST ${NAME}
            APPEND PROPERTY ENVIRONMENT "CROWY_SMOKE_FRAMES=${ARG_FRAMES}"
        )
    endif()
endfunction()

# windowed, and shows something. HEADLESS is the one exception that computes
# and exits, so it needs neither AppFramework nor a window.
function(crowy_declare_example NAME)
    cmake_parse_arguments(ARG "HEADLESS" "TEST;FRAMES" "SOURCES;DEPENDS" ${ARGN})

    set(FRAMEWORK FRAMEWORK)
    if(ARG_HEADLESS)
        set(FRAMEWORK "")
    endif()

    crowy_declare_executable(${NAME}
        TEST ${ARG_TEST}
        FRAMES ${ARG_FRAMES}
        ${FRAMEWORK}
    SOURCES
        ${ARG_SOURCES}
    DEPENDS
        ${ARG_DEPENDS}
    )
endfunction()

# windowed; pins a semantics difference between the backends, so the picture
# it draws is the assertion.
function(crowy_declare_spike NAME)
    cmake_parse_arguments(ARG "" "TEST;FRAMES" "SOURCES;DEPENDS" ${ARGN})

    crowy_declare_executable(${NAME}
        TEST ${ARG_TEST}
        FRAMES ${ARG_FRAMES}
        FRAMEWORK
    SOURCES
        ${ARG_SOURCES}
    DEPENDS
        ${ARG_DEPENDS}
    )
endfunction()

# headless; reads results back and exits nonzero when they drift from what the
# CPU says they should be.
function(crowy_declare_check NAME)
    cmake_parse_arguments(ARG "" "TEST" "SOURCES;DEPENDS" ${ARGN})

    crowy_declare_executable(${NAME}
        TEST ${ARG_TEST}
    SOURCES
        ${ARG_SOURCES}
    DEPENDS
        ${ARG_DEPENDS}
    )
endfunction()

# Crowy::Sample<NAME>. Sources are listed, not globbed: one sample folder holds
# several libraries and the executables built on them.
function(crowy_declare_sample_library NAME)
    cmake_parse_arguments(ARG
        "WHOLE_ARCHIVE;FP_CONTRACT_OFF"
        "TEST_DIRECTORY;TEST_LABELS"
        "SOURCES;INCLUDE;PUBLIC;PRIVATE;TEST_DEPENDS"
        ${ARGN}
    )

    if(ARG_WHOLE_ARCHIVE)
        set(LIBRARY CrowySample${NAME}Archive)
    else()
        set(LIBRARY CrowySample${NAME})
    endif()

    add_library(${LIBRARY} STATIC
        ${ARG_SOURCES}
    )

    target_include_directories(${LIBRARY}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        ${ARG_INCLUDE}
    )

    target_link_libraries(${LIBRARY}
    PUBLIC
        CrowyProjectInterface
        ${ARG_PUBLIC}
    PRIVATE
        ${ARG_PRIVATE}
    )

    if(ARG_FP_CONTRACT_OFF)
        crowy_fp_contract_off(${LIBRARY})
    endif()

    if(ARG_WHOLE_ARCHIVE)
        crowy_whole_archive(CrowySample${NAME} ${LIBRARY})
    endif()

    add_library(Crowy::Sample${NAME} ALIAS CrowySample${NAME})

    if(ARG_TEST_DIRECTORY AND CROWY_ENABLE_TEST)
        crowy_declare_test(Sample${NAME}
            DIRECTORY ${ARG_TEST_DIRECTORY}
            DEPENDS Crowy::Sample${NAME} ${ARG_TEST_DEPENDS}
            LABELS "${ARG_TEST_LABELS}"
        )
        if(ARG_FP_CONTRACT_OFF)
            crowy_fp_contract_off(CrowySample${NAME}Test)
        endif()
    endif()
endfunction()
