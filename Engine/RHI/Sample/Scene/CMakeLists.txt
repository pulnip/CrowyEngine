add_library(CrowySampleScene STATIC
    SampleScene.cpp
    CornellBox.cpp
    SkyboxFloor.cpp
)

target_include_directories(CrowySampleScene
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(CrowySampleScene
PUBLIC
    Crowy::Serialization
)

add_library(Crowy::SampleScene ALIAS CrowySampleScene)
