# A physics world written one way into render primitives. It knows no
# sample, so it can move out of the samples as it is.
add_library(SamplePhysicsSync STATIC
    BodyPrimitiveSync.cpp
)

target_include_directories(SamplePhysicsSync
PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SamplePhysicsSync
PUBLIC
    Crowy::Core
    Crowy::Physics
    Crowy::Render
)

# its tests repeat the library's arithmetic and compare bit for bit
crowy_fp_contract_off(SamplePhysicsSync)

if(CROWY_ENABLE_TEST)
    crowy_declare_test(SamplePhysicsSync
        DIRECTORY Test
        DEPENDS SamplePhysicsSync
        LABELS "unit"
    )
    crowy_fp_contract_off(CrowySamplePhysicsSyncTest)
endif()
