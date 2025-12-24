function(crowy_link_assets TARGET)
    set(ASSET_SOURCE "${CMAKE_SOURCE_DIR}/asset")
    set(ASSET_DEST "$<TARGET_FILE_DIR:${TARGET}>/asset")

    if(WIN32)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND cmd /c if not exist "${ASSET_DEST}" mklink /J "${ASSET_DEST}" "${ASSET_SOURCE}"
            COMMENT "Linking asset folder for ${TARGET}"
        )
    else()
        # macOS/Linux: symlink
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${ASSET_SOURCE}"
                "${ASSET_DEST}"
            COMMENT "Linking asset folder for ${TARGET}"
        )
    endif()
endfunction()