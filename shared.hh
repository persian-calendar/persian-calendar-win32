#pragma once
#include "warnings.hpp"
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define WINVER 0x0500 // XP support, and maybe 2000? Why not
IB_WARNING_DISABLE_CLANG_PUSH("-Wnonportable-system-include-path")
#include <windows.h>
IB_WARNING_DISABLE_CLANG_POP
#include <shellapi.h>
#include <dwmapi.h>
#include <Shlwapi.h>

#define APP_ID L"PersianCalendarWin32"
#define APP_TITLE L"تقویم فارسی"

// a magenta color used to create a color key for transparency in glass windows
constexpr COLORREF colorKey = RGB(0xFE, 0x01, 0xFD);
constexpr auto fresh_install_argument = L"/fresh";

struct LibraryLoader
{
private:
    HMODULE module;

public:
    LibraryLoader(const LibraryLoader &) = delete;
    LibraryLoader &operator=(const LibraryLoader &) = delete;
    // Let's just don't free, all we load is system libraries and they are always loaded anyway, so no need to free them.
    // ~LibraryLoader() { if (module) FreeLibrary(module); }
    LibraryLoader(const char *name) : module(GetModuleHandleA(name))
    {
        if (!module)
            module = LoadLibraryA(name);
    }

    template <typename T>
    T getProcedure(const char *name) const
    {
        return reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(module, name)));
    }
};

inline static auto
enable_hidpi() -> void
{
    LibraryLoader user32("user32");
    if (auto pSetProcessDpiAwarenessContext = user32.getProcedure<BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT value)>(
            "SetProcessDpiAwarenessContext"))
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else if (auto pSetProcessDPIAware = user32.getProcedure<BOOL(WINAPI *)()>("SetProcessDPIAware"))
        pSetProcessDPIAware();
}

template <typename T>
inline static auto
zero_memory(T &ptr, size_t size = sizeof(T)) -> void
{
    SecureZeroMemory(&ptr, size);
}

// This isn't always accurate in multimonitor with different DPIs setups
inline static auto
get_system_dpi() -> UINT
{
    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return 96;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return static_cast<UINT>(dpi);
}

inline static auto
get_build_number() -> DWORD
{
    if (auto pRtlGetVersion = LibraryLoader("ntdll.dll").getProcedure<LONG(WINAPI *)(PRTL_OSVERSIONINFOW lpVersionInformation)>("RtlGetVersion"))
    {
        RTL_OSVERSIONINFOW rovi;
        rovi.dwOSVersionInfoSize = sizeof rovi;
        if (pRtlGetVersion(&rovi) == 0)
            return rovi.dwBuildNumber;
    }
    return 0;
}

inline static auto
is_dark_mode_active() -> bool
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    if (get_build_number() < 17763)
        return false;
    auto pShouldAppsUseDarkMode = LibraryLoader("uxtheme.dll").getProcedure<bool(WINAPI *)()>(MAKEINTRESOURCEA(132)); // undocumented ShouldAppsUseDarkMode
    return pShouldAppsUseDarkMode && pShouldAppsUseDarkMode();
}

inline static void
enable_dark_mode_support()
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    DWORD build_number = get_build_number();
    if (build_number < 17763)
        return;
    LibraryLoader uxtheme("uxtheme.dll");
    if (build_number < 18362)
    {
        if (auto pAllowDarkModeForApp = uxtheme.getProcedure<bool(WINAPI *)(bool allow)>(
                MAKEINTRESOURCEA(135))) // undocumented AllowDarkModeForApp
            pAllowDarkModeForApp(true);
    }
    else
    {
        enum class PreferredAppMode : INT
        {
            Default,
            AllowDark,
            ForceDark,
            ForceLight,
            Max
        };
        if (auto pSetPreferredAppMode = uxtheme.getProcedure<INT(WINAPI *)(PreferredAppMode value)>(
                MAKEINTRESOURCEA(135))) // undocumented SetPreferredAppMode
            pSetPreferredAppMode(PreferredAppMode::AllowDark);
    }
}

inline static auto
set_immersive_dark_mode(HWND hWnd, BOOL darkMode) -> void
{
    if (auto set_attribute = LibraryLoader("dwmapi.dll").getProcedure<HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD)>("DwmSetWindowAttribute"))
        set_attribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof darkMode);
}

inline static auto
glass_window(HWND hWnd) -> void
{
    LibraryLoader dwmapi("dwmapi.dll");
    if (auto pDwmExtendFrameIntoClientArea = dwmapi.getProcedure<HRESULT(WINAPI *)(HWND, const MARGINS *)>(
            "DwmExtendFrameIntoClientArea"))
    {
        MARGINS margins = {.cxLeftWidth = -1, .cxRightWidth = -1, .cyTopHeight = -1, .cyBottomHeight = -1};
        pDwmExtendFrameIntoClientArea(hWnd, &margins);
    }
    if (auto pDwmSetWindowAttribute = dwmapi.getProcedure<HRESULT(WINAPI *)(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)>(
            "DwmSetWindowAttribute"))
    {
        int backdropType = DWMSBT_TRANSIENTWINDOW; // instead of Mica's DWMSBT_MAINWINDOW
        pDwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof backdropType);
    }
}

inline static auto
has_composition() -> bool
{
    if (auto pDwmIsCompositionEnabled = LibraryLoader("dwmapi.dll").getProcedure<HRESULT(WINAPI *)(BOOL * pfEnabled)>("DwmIsCompositionEnabled"))
    {
        BOOL result;
        pDwmIsCompositionEnabled(&result);
        return result;
    }
    return false;
}

inline static auto
toggle_exstyle(HWND hWnd, bool enable, long flag) -> void
{
    if (!hWnd)
        return;
    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (enable)
        exStyle |= flag;
    else
        exStyle &= ~flag;
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);
}

inline static auto
set_layered_window_attributes(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags) -> void
{
    if (auto pSetLayeredWindowAttributes = LibraryLoader("user32").getProcedure<BOOL(WINAPI *)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)>(
            "SetLayeredWindowAttributes"))
        pSetLayeredWindowAttributes(hwnd, crKey, bAlpha, dwFlags);
}
