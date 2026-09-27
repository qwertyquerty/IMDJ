#include "core/crash_guard.h"

#if defined(_MSC_VER)
#include <windows.h>
#endif

namespace imdj {

bool RunGuarded(const std::function<void()>& fn)
{
#if defined(_MSC_VER)
    __try {
        fn();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    fn();
    return true;
#endif
}

} // namespace imdj
