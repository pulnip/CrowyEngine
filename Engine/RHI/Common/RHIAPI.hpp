#pragma once

#ifdef USE_STATIC_RHI
    #define RHI_OVERRIDE
#else
    #define RHI_OVERRIDE override
#endif
