#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include "persian-calendar.h"

static HICON create_text_icon(HDC hdc, const wchar_t *text, bool black_background)
{
    const int size = 128; // GetSystemMetrics(SM_CXSMICON); oversized icon looks better
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hbmMask = CreateCompatibleBitmap(hdc, size, size);

    HDC memDC = CreateCompatibleDC(hdc);
    HGDIOBJ oldBmp = SelectObject(memDC, hbmColor);
    RECT rc = {0, 0, size, size};

    FillRect(memDC, &rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    HFONT hFont = CreateFontA(
        -size + 4, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, "Calibri");
    HGDIOBJ oldFont = SelectObject(memDC, hFont);

    DrawTextW(memDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(memDC, hbmMask);

    FillRect(memDC, &rc, reinterpret_cast<HBRUSH>(GetStockObject(black_background ? BLACK_BRUSH : WHITE_BRUSH)));

    SetTextColor(memDC, RGB(0, 0, 0));
    DrawTextW(memDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDC, oldFont);
    DeleteObject(hFont);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);

    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = hbmColor;
    iconInfo.hbmMask = hbmMask;

    HICON hIcon = CreateIconIndirect(&iconInfo);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return hIcon;
}

struct app_state_t
{
    NOTIFYICONDATAW *notify_icon_data;
    BOOL local_digits;
    BOOL black_background;
    HMENU menu;

    app_state_t(NOTIFYICONDATAW *notify_icon_data_) : notify_icon_data(notify_icon_data_), local_digits(true), black_background(true), menu(nullptr)
    {
    }
};

constexpr unsigned date_id = 1000;
constexpr unsigned first_separator_id = 1001;
constexpr unsigned local_digits_id = 1002;
constexpr unsigned black_background_id = 1003;
constexpr unsigned second_separator_id = 1004;
constexpr unsigned exit_id = 1005;
static void create_menu(app_state_t *state, wchar_t *date)
{
    HMENU menu = CreatePopupMenu();
    MENUITEMINFOW menu_item;
    SecureZeroMemory(&menu_item, sizeof(MENUITEMINFOW));
    menu_item.cbSize = sizeof(MENUITEMINFOW);
    menu_item.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | MIIM_DATA;
    {
        menu_item.fState = MFS_DISABLED;
        menu_item.wID = date_id;
        menu_item.dwTypeData = date;
        InsertMenuItemW(menu, date_id, TRUE, &menu_item);
    }
    InsertMenuA(menu, first_separator_id, MF_SEPARATOR, TRUE, nullptr);
    {
        menu_item.fState = state->local_digits ? MFS_CHECKED : 0;
        menu_item.wID = local_digits_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"اعداد فارسی");
        InsertMenuItemW(menu, local_digits_id, TRUE, &menu_item);
    }
    {
        menu_item.fState = state->black_background ? MFS_CHECKED : 0;
        menu_item.wID = black_background_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"پیش‌زمینهٔ سیاه آیکون");
        InsertMenuItemW(menu, black_background_id, TRUE, &menu_item);
    }
    InsertMenuA(menu, second_separator_id, MF_SEPARATOR, TRUE, nullptr);
    {
        menu_item.fState = 0;
        menu_item.wID = exit_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"خروج");
        InsertMenuItemW(menu, exit_id, TRUE, &menu_item);
    }
    HMENU old_menu = state->menu;
    state->menu = menu;
    if (old_menu)
        DestroyMenu(old_menu);
}

const static wchar_t *months[] = {
    L"فروردین",
    L"اردیبهشت",
    L"خرداد",
    L"تیر",
    L"مرداد",
    L"شهریور",
    L"مهر",
    L"آبان",
    L"آذر",
    L"دی",
    L"بهمن",
    L"اسفند",
};
const static wchar_t *weekdays[] = {
    L"شنبه",
    L"یکشنبه",
    L"دوشنبه",
    L"سه‌شنبه",
    L"چهارشنبه",
    L"پنجشنبه",
    L"جمعه",
};
static void update(HWND hwnd, app_state_t *state)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    persian_date_t date = gregorian_to_persian(st.wYear, st.wMonth, st.wDay);

    wchar_t day[3];
    wnsprintfW(day, sizeof(day) / sizeof(wchar_t), L"%d", date.day);
    wchar_t month[3];
    wnsprintfW(month, sizeof(month) / sizeof(wchar_t), L"%d", date.month);
    wchar_t year[5];
    wnsprintfW(year, sizeof(year) / sizeof(wchar_t), L"%d", date.year);
    if (state->local_digits)
    {
        day[0] += L'۰' - L'0';
        if (day[1])
            day[1] += L'۰' - L'0';
        month[0] += L'۰' - L'0';
        if (month[1])
            month[1] += L'۰' - L'0';
        year[0] += L'۰' - L'0';
        year[1] += L'۰' - L'0';
        year[2] += L'۰' - L'0';
        year[3] += L'۰' - L'0';
    }

    wnsprintfW(state->notify_icon_data->szTip, sizeof(state->notify_icon_data->szTip) / sizeof(wchar_t),
               L"%ls، %ls %ls/%ls %ls",
               weekdays[(static_cast<size_t>(st.wDayOfWeek) + 1) % 7], day, months[(date.month - 1) % 12], month, year);

    // szTip allocated string is both used for the tooltip and first item of the menu
    create_menu(state, state->notify_icon_data->szTip);

    HDC hdc = GetDC(hwnd);
    HICON icon = create_text_icon(hdc, day, state->black_background);
    ReleaseDC(hwnd, hdc);
    if (state->notify_icon_data->hIcon)
        DestroyIcon(state->notify_icon_data->hIcon);
    state->notify_icon_data->hIcon = icon;
    Shell_NotifyIconW(NIM_MODIFY, state->notify_icon_data);
}

#define appId "PersianCalendarWin32"
struct Registry
{
    Registry(const Registry &) = delete;
    void operator=(const Registry &) = delete;
    Registry() : key(nullptr)
    {
        LONG status = RegCreateKeyExA(
            HKEY_CURRENT_USER,
            "Software\\" appId,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE | KEY_READ,
            nullptr,
            &key,
            nullptr);
        if (status != ERROR_SUCCESS)
            key = nullptr;
    }

    void fill_app_state(app_state_t *state) const
    {
        if (!key)
            return;
        DWORD value = 0;
        DWORD size = sizeof(DWORD);
        DWORD type = 0;

        if (RegQueryValueExA(key, local_digits_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            state->local_digits = !!value;

        if (RegQueryValueExA(key, black_background_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            state->black_background = !!value;
    }

    void set_local_digits(bool value) const
    {
        set_bool(local_digits_key, value);
    }

    void set_black_background(bool value) const
    {
        set_bool(black_background_key, value);
    }

    ~Registry()
    {
        if (key)
            RegCloseKey(key);
    }

private:
    HKEY key;

    void set_bool(char *name, bool value) const
    {
        if (!key)
            return;
        DWORD dword = value ? 1 : 0;
        RegSetValueExA(
            key,
            name,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE *>(&dword),
            sizeof(DWORD));
    }

    constexpr static char *local_digits_key = const_cast<char *>("LocalDigits");
    constexpr static char *black_background_key = const_cast<char *>("BlackBackground");
};

static void enable_hidpi()
{
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (!user32)
        return;
    typedef BOOL(WINAPI * func_t)(DPI_AWARENESS_CONTEXT);
    func_t func = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
    if (func)
        func(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    FreeLibrary(user32);
}

static void enable_dark_mode_support()
{
    HMODULE uxtheme = LoadLibraryA("uxtheme.dll");
    if (!uxtheme)
        return;
    typedef INT(WINAPI * func_t)(INT); // undocumented SetPreferredAppMode's signature
    func_t func = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(uxtheme, MAKEINTRESOURCEA(135))));
    if (func)
        func(/*Allow dark*/ 1);
    FreeLibrary(uxtheme);
}

const unsigned notifyClickId = WM_USER + 1;
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    app_state_t *state = reinterpret_cast<app_state_t *>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(EXIT_SUCCESS);
        return 0;
    case WM_TIMER:
        update(hwnd, state);
        return 0;
    case notifyClickId:
        if (lparam == WM_RBUTTONUP)
        {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(state->menu, TPM_RIGHTALIGN | TPM_RIGHTBUTTON | TPM_LAYOUTRTL,
                           p.x, p.y, 0, hwnd, nullptr);
        }
        else if (lparam == WM_LBUTTONUP)
        {
            CreateWindowExA(WS_EX_TOPMOST, "MainWindow", "Main Window",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                            CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, hwnd, nullptr, GetModuleHandleA(nullptr), nullptr);
        }
        return 0;
    case WM_COMMAND:
        if (wparam == local_digits_id)
        {
            state->local_digits = !state->local_digits;
            update(hwnd, state);
            Registry().set_local_digits(state->local_digits);
            return 0;
        }
        else if (wparam == black_background_id)
        {
            state->black_background = !state->black_background;
            update(hwnd, state);
            Registry().set_black_background(state->black_background);
            return 0;
        }
        else if (wparam == exit_id)
        {
            PostQuitMessage(EXIT_SUCCESS);
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static void apply_dark_mode_to_window(HWND hwnd)
{
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
    if (!dwmapi)
        return;
    
    typedef HRESULT(WINAPI * func_t)(HWND, DWORD, LPCVOID, DWORD);
    func_t func = reinterpret_cast<func_t>(
        reinterpret_cast<void *>(GetProcAddress(dwmapi, "DwmSetWindowAttribute")));
    
    if (func)
    {
        BOOL useDarkMode = TRUE;
        func(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDarkMode, sizeof(useDarkMode));
    }
    
    FreeLibrary(dwmapi);
}

static void enable_dark_mode_for_window(HWND hwnd)
{
    HMODULE uxtheme = LoadLibraryA("uxtheme.dll");
    if (!uxtheme)
        return;
    
    // Undocumented AllowDarkModeForWindow (ordinal 133)
    typedef BOOL(WINAPI * func_t)(HWND, BOOL);
    func_t func = reinterpret_cast<func_t>(
        reinterpret_cast<void *>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(133))));
    
    if (func)
        func(hwnd, TRUE);
    
    FreeLibrary(uxtheme);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        apply_dark_mode_to_window(hwnd);
        CreateWindowExA(
            0,
            "BUTTON",
            "Click Me",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            150, 120, 100, 30,
            hwnd,
            reinterpret_cast<HMENU>(1001),
            GetModuleHandleA(nullptr),
            nullptr);
        enable_dark_mode_for_window(hwnd);
        return 0;
    }
    case WM_CTLCOLORBTN:
    {
        HDC hdcButton = reinterpret_cast<HDC>(wparam);
        SetBkColor(hdcButton, GetSysColor(COLOR_WINDOW));
        SetTextColor(hdcButton, GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_ERASEBKGND:
    {
        HDC hdc = reinterpret_cast<HDC>(wparam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        return 1;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == 1001)
        {
            MessageBoxA(hwnd, "Button clicked!", "Info", MB_OK);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

// https://web.archive.org/web/20190205041452/https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483
extern "C" IMAGE_DOS_HEADER __ImageBase;

extern "C" [[noreturn]] void start();
void start()
{
    HANDLE mutex = CreateMutexA(nullptr, 0, const_cast<char *>(appId));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(EXIT_FAILURE);
    HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    {
        WNDCLASSEXA wc;
        SecureZeroMemory(&wc, sizeof(WNDCLASSEXA));
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.hInstance = module;
        wc.lpfnWndProc = MainWndProc;
        wc.lpszClassName = "MainWindow";
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursorA(nullptr, MAKEINTRESOURCEA(32512));
        RegisterClassExA(&wc);
    }

    {
        WNDCLASSEXA wc;
        SecureZeroMemory(&wc, sizeof(WNDCLASSEXA));
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.hInstance = module;
        wc.lpfnWndProc = WndProc;
        wc.lpszClassName = appId;
        if (!RegisterClassExA(&wc))
            ExitProcess(EXIT_FAILURE);
    }

    HWND hwnd = CreateWindowExA(0, appId, nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, module, nullptr);
    if (!hwnd)
        ExitProcess(EXIT_FAILURE);

    // Initiation
    NOTIFYICONDATAW notify_icon_data;
    SecureZeroMemory(&notify_icon_data, sizeof(NOTIFYICONDATAW));
    app_state_t state(&notify_icon_data);
    {
        enable_hidpi();
        enable_dark_mode_support();
        notify_icon_data.cbSize = sizeof(NOTIFYICONDATAW);
        notify_icon_data.uCallbackMessage = notifyClickId;
        notify_icon_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
        Registry().fill_app_state(&state);
        state.notify_icon_data->hWnd = hwnd;
        Shell_NotifyIconW(NIM_ADD, state.notify_icon_data);
        update(hwnd, &state);
        SetTimer(hwnd, 1 /*timer id*/, 60000, nullptr);
    }

    // Main loop
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    // Finalize
    {
        Shell_NotifyIconW(NIM_DELETE, state.notify_icon_data);
        DestroyIcon(state.notify_icon_data->hIcon);
    }

    UnregisterClassA(appId, module);
    ExitProcess(msg.wParam);
}
