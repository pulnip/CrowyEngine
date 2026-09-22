#pragma once

#include "ClassRegistry.hpp"
#include "CommandPort.hpp"
#include "PropertyWrite.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    struct Exposure {
        void* target = nullptr;
        const TypeDesc* desc = nullptr;
        DirtyCallback onDirty;
    };

    using Exposures = StringHashMap<Exposure>;

    // the reflection verbs, each generic over whatever was exposed
    void listObjects(const Exposures&, const DOM::Value& args, Reply);
    void describeObject(const Exposures&, const DOM::Value& args, Reply);
    void getProperty(const Exposures&, const DOM::Value& args, Reply);
    void setProperty(const Exposures&, const DOM::Value& args, Reply);
}
