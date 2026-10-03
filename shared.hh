#pragma once
#include "warnings.hpp"
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define WINVER 0x0500 // XP support, and maybe 2000? Why not
IB_WARNING_DISABLE_CLANG_PUSH("-Wnonportable-system-include-path")
#include <windows.h>
IB_WARNING_DISABLE_CLANG_POP
#include <shellapi.h>

#define APP_ID L"PersianCalendarWin32"

struct LibraryLoader
{
private:
    HMODULE m_module;

    static auto getModuleWithFallback(const char *name) -> HMODULE
    {
        HMODULE module = GetModuleHandleA(name);
        return module ? module : LoadLibraryA(name);
    }

public:
    LibraryLoader(const LibraryLoader &) = delete;
    void operator=(const LibraryLoader &) = delete;
    LibraryLoader(const char *name) : m_module(getModuleWithFallback(name)) {}
    // Let's just don't free, all we load is system libraries and they are always loaded anyway, so no need to free them.
    // ~LibraryLoader() { if (module) FreeLibrary(module); }

    template <typename T>
    auto getProcedure(const char *procName)
    {
        return reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(m_module, procName))); // NOLINT(bugprone-casting-through-void)
    }
};

inline void enable_hidpi()
{
    LibraryLoader user32("user32");
    auto pSetProcessDpiAwarenessContext = user32.getProcedure<BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT value)>(
        "SetProcessDpiAwarenessContext");
    if (pSetProcessDpiAwarenessContext)
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else
    {
        auto pSetProcessDPIAware = user32.getProcedure<BOOL(WINAPI *)()>("SetProcessDPIAware");
        if (pSetProcessDPIAware)
            pSetProcessDPIAware();
    }
}

template <typename T>
inline void zero_memory(T &ptr, size_t size = sizeof(T))
{
    SecureZeroMemory(&ptr, size);
}

constexpr unsigned date_id = 1000;
constexpr unsigned first_separator_id = 1001;
constexpr unsigned local_digits_id = 1002;
constexpr unsigned black_background_id = 1003;
constexpr unsigned second_separator_id = 1004;
constexpr unsigned show_widget_id = 1005;
constexpr unsigned fixed_widget_placement_id = 1006;
constexpr unsigned always_on_top_widget_id = 1007;
constexpr unsigned third_separator_id = 1008;
constexpr unsigned date_converter_id = 1009;
constexpr unsigned fourth_separator_id = 1010;
constexpr unsigned exit_id = 1011;
