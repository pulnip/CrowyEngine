# MintChoco's paint as PaintLab runs it: the CPU half, ported from the game's
# C++ in its centimetre frame, and knowing nothing of the RHI
add_library(SamplePaint STATIC
    PaintAtlasBaker.cpp
    PaintBrushProfile.cpp
    PaintCellGrid.cpp
    PaintIslandLayout.cpp
    PaintMeshes.cpp
    PaintSceneQuery.cpp
    PaintStage.cpp
    PaintSurface.cpp
    PaintWorld.cpp
)

target_include_directories(SamplePaint
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SamplePaint
PUBLIC
    Crowy::Core
)

# Unreal's float rounding, fused nowhere
crowy_fp_contract_off(SamplePaint)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SamplePaint
        DIRECTORY Test
        DEPENDS SamplePaint
        LABELS "unit"
    )
    crowy_fp_contract_off(CrowySamplePaintTest)
endif()
