set(ASSET_SOURCE "${CMAKE_SOURCE_DIR}/asset")
set(ASSET_DEST "${CMAKE_BINARY_DIR}/bin/asset")

if(EXISTS "${ASSET_DEST}")
    return()
endif()

# try to link asset folder
if(WIN32)
    execute_process(
        COMMAND cmd /c mklink /J "${ASSET_DEST}" "${ASSET_SOURCE}"
        RESULT_VARIABLE LINK_RESULT
    )
else()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${ASSET_SOURCE}" "${ASSET_DEST}"
        RESULT_VARIABLE LINK_RESULT
    )
endif()

# fallback: copy asset folder
if(NOT LINK_RESULT EQUAL 0)
    message(MESSAGE "Symlink failed, falling back to copy...")
    file(COPY "${ASSET_SOURCE}" DESTINATION "${CMAKE_BINARY_DIR}/bin")
endif()
