#include "WindowManager.h"
#include "HyprlandClient.h"
#include "HyprlandWindowManager.h"
#include "NullWindowManager.h"

std::unique_ptr<WindowManager> WindowManager::create()
{
    if (HyprlandClient::available())
        return std::make_unique<HyprlandWindowManager>();
    return std::make_unique<NullWindowManager>();
}
