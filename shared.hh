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

// a magenta color used to create a color key for transparency in glass windows
constexpr COLORREF colorKey = RGB(0xFE, 0x01, 0xFD);

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
    if (auto pSetProcessDpiAwarenessContext = user32.getProcedure<BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT value)>(
            "SetProcessDpiAwarenessContext"))
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else if (auto pSetProcessDPIAware = user32.getProcedure<BOOL(WINAPI *)()>("SetProcessDPIAware"))
        pSetProcessDPIAware();
}

template <typename T>
inline void zero_memory(T &ptr, size_t size = sizeof(T))
{
    SecureZeroMemory(&ptr, size);
}

// This isn't always accurate in multimonitor with different DPIs setups
inline auto get_system_dpi() -> UINT
{
    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return 96;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return static_cast<UINT>(dpi);
}

inline auto get_build_number() -> DWORD
{
    auto pRtlGetVersion = LibraryLoader("ntdll.dll").getProcedure<LONG(WINAPI *)(PRTL_OSVERSIONINFOW lpVersionInformation)>("RtlGetVersion");
    if (pRtlGetVersion)
    {
        RTL_OSVERSIONINFOW rovi;
        rovi.dwOSVersionInfoSize = sizeof(rovi);
        if (pRtlGetVersion(&rovi) == 0)
            return rovi.dwBuildNumber;
    }
    return 0;
}

inline auto is_dark_mode_active() -> bool
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    if (get_build_number() < 17763)
        return false;
    auto pShouldAppsUseDarkMode = LibraryLoader("uxtheme.dll").getProcedure<bool(WINAPI *)()>(MAKEINTRESOURCEA(132)); // undocumented ShouldAppsUseDarkMode
    return pShouldAppsUseDarkMode && pShouldAppsUseDarkMode();
}

inline void enable_dark_mode_support()
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    DWORD build_number = get_build_number();
    if (build_number < 17763)
        return;
    LibraryLoader uxtheme("uxtheme.dll");
    if (build_number < 18362)
    {
        auto pAllowDarkModeForApp = uxtheme.getProcedure<bool(WINAPI *)(bool allow)>(
            MAKEINTRESOURCEA(135)); // undocumented AllowDarkModeForApp
        if (pAllowDarkModeForApp)
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
        auto pSetPreferredAppMode = uxtheme.getProcedure<INT(WINAPI *)(PreferredAppMode value)>(
            MAKEINTRESOURCEA(135)); // undocumented SetPreferredAppMode
        if (pSetPreferredAppMode)
            pSetPreferredAppMode(PreferredAppMode::AllowDark);
    }
}

inline void set_immersive_dark_mode(HWND hWnd, BOOL darkMode)
{
    if (auto set_attribute = LibraryLoader("dwmapi.dll").getProcedure<HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD)>("DwmSetWindowAttribute"))
        set_attribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof darkMode);
}

inline void glass_window(HWND hWnd)
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
        pDwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
    }
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
