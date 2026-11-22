#include "Library.h"
#include "IGuiModule.h"
#include "Options.h"
#include "Theme.h"

#include "lua.h"
#include "lualib.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

namespace Luwow::Theme {

namespace {

ILuauHost* host = nullptr;

// The native handle of the gui Window at index, erroring for anything else or a closed window
void* windowArg(lua_State* L, int index) {
    luaL_checkany(L, index);
    lua_pushvalue(L, index);
    Luwow::Engine::Message message;
    message.topic = Gui::Topics::NativeHandle;
    message.state = L;
    bool handled = host->request(message);
    lua_pop(L, 1);

    char type[32] = {};
    uintptr_t address = 0;
    bool window = handled && message.result == 1
        && std::sscanf(message.data.c_str(), "%31s %" SCNuPTR, type, &address) == 2
        && std::strcmp(type, "Window") == 0;
    if (!window) luaL_typeerror(L, index, "Window");
    if (!address) luaL_error(L, "the window is closed");
    return reinterpret_cast<void*>(address);
}

void runWork(void* work) {
    (*static_cast<const std::function<void()>*>(work))();
}

// Runs work on the gui library's main thread, where its windows are used, or right here without the gui library
void onMainThread(lua_State* L, const std::function<void()>& work) {
    Luwow::Engine::Message message;
    message.topic = Gui::Topics::RunOnMainThread;
    message.state = L;
    message.data = std::to_string(reinterpret_cast<uintptr_t>(&runWork)) + " " + std::to_string(reinterpret_cast<uintptr_t>(&work));
    if (!host->request(message)) work();
    else if (message.result != 1) luaL_error(L, "the gui library's main loop has ended");
}

// theme.configure(options)
int configure(lua_State* L) {
    Options options = readOptions(L, 1);
    onMainThread(L, [&] { configureTheme(options); });
    return 0;
}

// theme.apply(window, options?)
int apply(lua_State* L) {
    void* window = windowArg(L, 1);
    Options options = lua_isnoneornil(L, 2) ? Options() : readOptions(L, 2);
    onMainThread(L, [&] { applyTheme(window, options); });
    return 0;
}

LuauExport exports[] = {
    { "configure", configure },
    { "apply", apply },
    { nullptr, nullptr }
};

} // namespace

ILuauModule* Library::initialize(ILuauHost* luauHost) {
    host = luauHost;
    startPlatform();
    return new Library();
}

RunMode Library::getRunMode() const {
    return LUWOW_MODULE_RUN_MODE;
}

const char* Library::getModuleName() const {
    return "theme";
}

const char* Library::getModuleAlias() const {
    return "Luwow";
}

const LuauExport* Library::getExports() const {
    return exports;
}

} // namespace Luwow::Theme

LUWOW_REGISTER_MODULE(Luwow::Theme::Library)
