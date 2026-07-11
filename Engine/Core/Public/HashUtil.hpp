#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    inline void hashCombine(usize& seed, usize v) {
        seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<typename... Args>
    inline usize hashAll(const Args&... args) {
        usize h = 0;
        (hashCombine(h, std::hash<Args>{}(args)), ...);
        return h;
    }
}

template<>
struct std::hash<Crowy::Vec2>{
    std::size_t operator()(const Crowy::Vec2& v) const noexcept{
        using namespace Crowy;

        return hashAll(v.x, v.y);
    }
};

template<>
struct std::hash<Crowy::Vec3>{
    std::size_t operator()(const Crowy::Vec3& v) const noexcept{
        using namespace Crowy;

        return hashAll(v.x, v.y, v.z);
    }
};

template<>
struct std::hash<Crowy::Vec4>{
    std::size_t operator()(const Crowy::Vec4& v) const noexcept{
        using namespace Crowy;

        return hashAll(v.x, v.y, v.z, v.w);
    }
};

template<>
struct std::hash<Crowy::Size2D>{
    std::size_t operator()(const Crowy::Size2D& s) const noexcept{
        using namespace Crowy;

        return hashAll(s.x, s.y);
    }
};

template<>
struct std::hash<Crowy::Size3D>{
    std::size_t operator()(const Crowy::Size3D& s) const noexcept{
        using namespace Crowy;

        return hashAll(s.x, s.y, s.z);
    }
};
