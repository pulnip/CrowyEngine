smol_declare_module(Resource)

target_link_libraries(CrowyResource
PUBLIC
    Crowy::Core
    Crowy::RHI
PRIVATE
    Crowy::Serialization
    Crowy::Log
)
