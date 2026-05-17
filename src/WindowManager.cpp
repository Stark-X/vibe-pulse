#include "WindowManager.h"
#include "NullWindowManager.h"

#ifdef Q_OS_LINUX
#  include "HyprlandClient.h"
#  include "HyprlandWindowManager.h"
#endif

#ifdef Q_OS_MACOS
#  include "MacOSWindowManager.h"
#endif

std::unique_ptr<WindowManager> WindowManager::create()
{
#ifdef Q_OS_LINUX
    if (HyprlandClient::available())
        return std::make_unique<HyprlandWindowManager>();
#endif
#ifdef Q_OS_MACOS
    return std::make_unique<MacOSWindowManager>();
#endif
    return std::make_unique<NullWindowManager>();
}
