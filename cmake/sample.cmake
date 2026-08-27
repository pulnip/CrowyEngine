# Executable taxonomy. Every executable in the tree is one of three kinds, and
# the kind is legible from its folder, its name and its ctest label alone:
#
#   Example  no suffix   windowed, shows something
#   Spike    *Spike      cross-backend semantics verification, windowed
#   Check    *Check      headless logic guard
#
# "smoke" stays the umbrella label over all three, so one kind runs alone with
#   ctest --test-dir build -C Debug -L check
# and everything windowed and headless alike still runs with -L smoke.

# App and the Main<T> entry template windowed executables are built on
set(CROWY_APP_FRAMEWORK_DIR "${CMAKE_SOURCE_DIR}/Engine/RHI/Sample")

function(crowy_declare_executable NAME)
    cmake_parse_arguments(ARG "FRAMEWORK" "CATEGORY" "SOURCES;DEPENDS" ${ARGN})

    add_executable(${NAME}
        ${ARG_SOURCES}
    )

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

    # launch-and-watch smoke test (see Tools/smoke_run.sh|.ps1);
    # registered here so every new executable gets one automatically.
    # duration and frame capture come from CROWY_SMOKE_* environment
    # variables read by the scripts, so no values are baked in here.
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
    if(DEFINED SMOKE_COMMAND)
        add_test(
            NAME ${NAME}Smoke
            COMMAND ${SMOKE_COMMAND}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(${NAME}Smoke
        PROPERTIES
            LABELS "smoke;${ARG_CATEGORY}"
            # windowed apps fight over focus, and the debug layer slows
            # everything down; one at a time keeps results stable
            RUN_SERIAL TRUE
            # generous: the watch window is env-configurable
            TIMEOUT 300
        )
    endif()
endfunction()

# windowed, and shows something. HEADLESS is the one exception that computes
# and exits, so it needs neither AppFramework nor a window.
function(crowy_declare_example NAME)
    cmake_parse_arguments(ARG "HEADLESS" "" "SOURCES;DEPENDS" ${ARGN})

    set(FRAMEWORK FRAMEWORK)
    if(ARG_HEADLESS)
        set(FRAMEWORK "")
    endif()

    crowy_declare_executable(${NAME}
        CATEGORY example
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
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    crowy_declare_executable(${NAME}
        CATEGORY spike
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
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})

    crowy_declare_executable(${NAME}
        CATEGORY check
    SOURCES
        ${ARG_SOURCES}
    DEPENDS
        ${ARG_DEPENDS}
    )
endfunction()
