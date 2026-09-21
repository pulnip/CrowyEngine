#pragma once

#include <functional>

namespace Crowy
{
    // Fired after every write into a reflected target.
    // Every writer - the property panel, the remote port -
    // notifies through the target's own callback, never around it.
    using DirtyCallback = std::function<void()>;
}
