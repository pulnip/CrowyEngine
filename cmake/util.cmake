function(crowy_link_folder TARGET)
    foreach(FOLDER IN LISTS ARGN)
        set(FOLDER_SOURCE "${CMAKE_SOURCE_DIR}/${FOLDER}")
        set(FOLDER_LINK "$<TARGET_FILE_DIR:${TARGET}>/${FOLDER}")

        if(WIN32)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND cmd /c if not exist "${FOLDER_LINK}" mklink /J "${FOLDER_LINK}" "${FOLDER_SOURCE}"
                COMMENT "Linking ${FOLDER} folder for ${TARGET}"
            )
        else()
            # macOS/Linux: symlink
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "${FOLDER_SOURCE}"
                    "${FOLDER_LINK}"
                COMMENT "Linking ${FOLDER} folder for ${TARGET}"
            )
        endif()
    endforeach()
endfunction()