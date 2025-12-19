if(NOT DEFINED RENDER_BACKEND)
    set(RENDER_BACKEND "D3D12")
endif()

if(RENDER_BACKEND STREQUAL "D3D12")
    find_program(SHADER_COMPILER NAMES fxc HINTS
        "$ENV{VCToolsInstallDir}/bin/Hostx64/x64"
        "$ENV{WindowsSdkBinPath}/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
    )
    if(NOT SHADER_COMPILER)
        message(FATAL_ERROR
            "fxc.exe (HLSL Shader Compiler) not found!"
            "Please check your Windows SDK installation."
        )
    endif()
elseif(RENDER_BACKEND STREQUAL "Metal")
    message(FATAL_ERROR "Metal is not supported on Windows.")
elseif(RENDER_BACKEND STREQUAL "OpenGL")
    set(SHADER_COMPILER glslangValidator)
    find_package(OpenGL REQUIRED)
    find_package(glad REQUIRED)
else()
    message(FATAL_ERROR "Invalid RENDER_BACKEND ${RENDER_BACKEND}")
endif()


