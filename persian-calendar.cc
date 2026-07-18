#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>

#include "persian-calendar.h"

// https://web.archive.org/web/20190205041452/https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483
extern "C" IMAGE_DOS_HEADER __ImageBase;
#define hInst (reinterpret_cast<HMODULE>(&__ImageBase))

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
constexpr unsigned converter_from_persian_id = 1005;
constexpr unsigned converter_from_gregorian_id = 1006;
constexpr unsigned third_separator_id = 1007;
constexpr unsigned exit_id = 1008;
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
        menu_item.wID = converter_from_persian_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"تبدیل تاریخ از شمسی");
        InsertMenuItemW(menu, converter_from_persian_id, TRUE, &menu_item);
    }
    {
        menu_item.fState = 0;
        menu_item.wID = converter_from_gregorian_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"تبدیل تاریخ از میلادی");
        InsertMenuItemW(menu, converter_from_gregorian_id, TRUE, &menu_item);
    }
    InsertMenuA(menu, third_separator_id, MF_SEPARATOR, TRUE, nullptr);
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

const static wchar_t *persian_months[] = {
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

static const wchar_t *gregorian_months[] = {
    L"ژانویه",
    L"فوریه",
    L"مارس",
    L"آوریل",
    L"مه",
    L"ژوئن",
    L"ژوئیه",
    L"اوت",
    L"سپتامبر",
    L"اکتبر",
    L"نوامبر",
    L"دسامبر",
};

constexpr unsigned dlg_day_combo_id = 2001;
constexpr unsigned dlg_month_combo_id = 2002;
constexpr unsigned dlg_year_combo_id = 2003;

enum converter_mode_t
{
    INVALID,
    PERSIAN,
    GREGORIAN
};

struct formatted_number_t
{
    wchar_t value[8];
};
static formatted_number_t format_number(unsigned number, bool local_digits = true)
{
    formatted_number_t result;
    constexpr unsigned size = sizeof(result.value) / sizeof(wchar_t);
    wnsprintfW(result.value, size, L"%d", number);
    if (local_digits)
        for (unsigned i = 0; i < size && result.value[i % size]; ++i)
            result.value[i % size] += L'۰' - L'0';
    return result;
}

static void format_date(
    BOOL local_digits, converter_mode_t mode, date_triplet_t date, unsigned dayOfWeek, wchar_t *result, int result_size)
{
    wnsprintfW(result, result_size,
               L"%ls، %ls %ls(%ls) %ls",
               weekdays[(dayOfWeek + 3) % 7],
               format_number(date.day, local_digits).value,
               mode == PERSIAN ? persian_months[(date.month - 1) % 12] : gregorian_months[(date.month - 1) % 12],
               format_number(date.month, local_digits).value,
               format_number(date.year, local_digits).value);
}

static unsigned today_in_days()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return gregorian_to_days({static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth), static_cast<unsigned>(st.wDay)});
}

static bool date_equal(date_triplet_t x, date_triplet_t y)
{
    return x.year == y.year && x.month == y.month && x.day == y.day;
}

static void do_conversion(HWND hwnd, converter_mode_t mode)
{
    date_triplet_t input_date{
        static_cast<unsigned>(SendDlgItemMessageW(hwnd, dlg_year_combo_id, CB_GETCURSEL, 0, 0)) +
            static_cast<unsigned>(GetWindowLongPtr(GetDlgItem(hwnd, dlg_year_combo_id), GWLP_USERDATA)),
        static_cast<unsigned>(SendDlgItemMessageW(hwnd, dlg_month_combo_id, CB_GETCURSEL, 0, 0)) + 1,
        static_cast<unsigned>(SendDlgItemMessageW(hwnd, dlg_day_combo_id, CB_GETCURSEL, 0, 0)) + 1};
    unsigned days = mode == PERSIAN ? persian_to_days(input_date) : gregorian_to_days(input_date);
    date_triplet_t converted_date = mode == PERSIAN ? days_to_gregorian(days) : days_to_persian(days);

    if (!date_equal(input_date, mode == PERSIAN ? days_to_persian(days) : days_to_gregorian(days)))
    {
        SetWindowTextW(hwnd, L"روز انتخاب‌شده نامعتبر است");
        // wchar_t result[128];
        // wnsprintfW(result, sizeof(result) / sizeof(wchar_t), L"%d %d %d", day, month, year);
        // SetWindowTextW(hwnd, result);
        return;
    }

    wchar_t formatted_date[128];
    format_date(TRUE, mode == PERSIAN ? GREGORIAN : PERSIAN,
                converted_date,
                days % 7,
                formatted_date, sizeof(formatted_date) / sizeof(wchar_t));
    wchar_t suffix[128];
    {
        unsigned today_days = today_in_days();
        if (days < today_days)
            wnsprintfW(suffix, sizeof(suffix) / sizeof(wchar_t), L"%ls روز در گذشته",
                       format_number(today_days - days).value);
        else if (days > today_days)
            wnsprintfW(suffix, sizeof(suffix) / sizeof(wchar_t), L"%ls روز در آینده",
                       format_number(days - today_days).value);
        else
            wnsprintfW(suffix, sizeof(suffix) / sizeof(wchar_t), L"امروز");
    }
    wchar_t result[128];
    wnsprintfW(result, sizeof(result) / sizeof(wchar_t), L"%ls، %ls", formatted_date, suffix);
    SetWindowTextW(hwnd, result);
}

static UINT GetSystemDpi()
{
    using func_t = UINT(WINAPI *)();
    auto pGetDpiForSystem = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForSystem")));
    return pGetDpiForSystem ? pGetDpiForSystem() : 96;
}

static int dp(UINT dpi, int value)
{
    return MulDiv(value, static_cast<int>(dpi), 72);
}
constexpr int padding = 8;
constexpr int combo_width = 72;
constexpr int combo_height = 42;

static BOOL IsDarkModeActive()
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExA(key, "AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
    }
    return value == 0;
}

// In remembrance of old era Windows color/chroma keying,
// * https://devblogs.microsoft.com/oldnewthing/20251014-00/?p=111681
// * https://learn.microsoft.com/en-us/windows/win32/directshow/overlay-mixer-filter#:~:text=magenta%20for%20older%20256%2Dcolor%20cards
#define APP_LWA_COLORKEY (RGB(0xFF, 0x00, 0xFF))

static void ApplyAeroAndMica(HWND hDlg)
{
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (!hDwm)
        return;
    {
        using func_t = HRESULT(WINAPI *)(HWND, MARGINS *);
        auto pDwmExtendFrameIntoClientArea = reinterpret_cast<func_t>(reinterpret_cast<void *>(
            GetProcAddress(hDwm, "DwmExtendFrameIntoClientArea")));
        MARGINS margins = {-1, -1, -1, -1};
        if (pDwmExtendFrameIntoClientArea)
            pDwmExtendFrameIntoClientArea(hDlg, &margins);
    }

    {
        using func_t = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
        auto pDwmSetWindowAttribute = reinterpret_cast<func_t>(reinterpret_cast<void *>(
            GetProcAddress(hDwm, "DwmSetWindowAttribute")));
        if (pDwmSetWindowAttribute)
        {
            BOOL darkMode = IsDarkModeActive();
            if (darkMode)
                pDwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
            int backdropType = DWMSBT_MAINWINDOW;
            pDwmSetWindowAttribute(hDlg, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
        }
    }
    FreeLibrary(hDwm);
    InvalidateRect(hDlg, nullptr, TRUE);
}

static LRESULT CALLBACK ConverterDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    converter_mode_t mode = static_cast<converter_mode_t>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_CREATE:
        // case WM_SETTINGCHANGE:
        ApplyAeroAndMica(hwnd);
        break;
    case WM_SHOWWINDOW:
    {
        UINT dpi = GetSystemDpi();
        auto d = [dpi](int value)
        {
            return dp(dpi, value);
        };

        HMODULE hUxtheme = GetModuleHandleA("uxtheme.dll");
        using func1_t = HRESULT(WINAPI *)(HWND, LPCWSTR, LPCWSTR);
        auto pSetWindowTheme = reinterpret_cast<func1_t>(reinterpret_cast<void *>(
            GetProcAddress(hUxtheme, "SetWindowTheme")));

        using func2_t = bool(WINAPI *)(HWND hWnd, BOOL allow);
        auto pAllowDarkModeForWindow = reinterpret_cast<func2_t>(
            reinterpret_cast<void *>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133))));

        BOOL darkMode = IsDarkModeActive();

        LOGFONTW lf;
        {
            SecureZeroMemory(&lf, sizeof(LOGFONTW));
            lf.lfHeight = -d(12);
            lf.lfWeight = 400;
            lf.lfOutPrecision = 1;
            lf.lfClipPrecision = 2;
            lf.lfQuality = 1;
            lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
            wnsprintfW(lf.lfFaceName, sizeof(lf.lfFaceName) / sizeof(wchar_t), L"Segoe UI");
        }
        HFONT font = CreateFontIndirectW(&lf);

        date_triplet_t date;
        {
            unsigned today = today_in_days();
            if (mode == GREGORIAN)
                date = days_to_gregorian(today);
            else if (mode == PERSIAN)
                date = days_to_persian(today);
            else
                return 0;
        }
        {
            HWND hDay = CreateWindowExA(0, "COMBOBOX", nullptr,
                                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                        d(padding), d(padding), d(combo_width), d(combo_height), hwnd,
                                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_day_combo_id)), hInst, nullptr);
            for (unsigned i = 1; i <= 31; ++i)
                SendMessageW(hDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format_number(i).value));
            SendMessageW(hDay, CB_SETCURSEL, date.day - 1, 0);
            SendMessageW(hDay, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (pAllowDarkModeForWindow && darkMode)
                pAllowDarkModeForWindow(hDay, true);
            if (pSetWindowTheme && darkMode)
                pSetWindowTheme(hDay, L"CFD", nullptr);
        }
        {
            HWND hMonth = CreateWindowExA(0, "COMBOBOX", nullptr,
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                          d(2 * padding + combo_width), d(padding), d(combo_width), d(combo_height), hwnd,
                                          reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_month_combo_id)), hInst, nullptr);
            for (unsigned i = 1; i <= 12; ++i)
            {
                wchar_t buf[32];
                wnsprintfW(buf, sizeof(buf) / sizeof(wchar_t), L"%ls (%ls)",
                           mode == PERSIAN ? persian_months[(i - 1) % 12] : gregorian_months[(i - 1) % 12],
                           format_number(i).value);
                SendMessageW(hMonth, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
            }
            SendMessageW(hMonth, CB_SETCURSEL, date.month - 1, 0);
            SendMessageW(hMonth, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (pAllowDarkModeForWindow && darkMode)
                pAllowDarkModeForWindow(hMonth, true);
            if (pSetWindowTheme && darkMode)
                pSetWindowTheme(hMonth, L"CFD", nullptr);
        }
        {
            HWND hYear = CreateWindowExA(0, "COMBOBOX", nullptr,
                                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         d(3 * padding + 2 * combo_width), d(padding), d(combo_width), d(combo_height), hwnd,
                                         reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_year_combo_id)), hInst, nullptr);
            constexpr unsigned combobox_years = 100;
            SetWindowLongPtr(hYear, GWLP_USERDATA, static_cast<LONG_PTR>(date.year - combobox_years / 2));
            for (unsigned i = 0; i <= combobox_years; ++i)
                SendMessageW(hYear, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format_number(date.year + i - combobox_years / 2).value));
            SendMessageW(hYear, CB_SETCURSEL, combobox_years / 2, 0);
            SendMessageW(hYear, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (pAllowDarkModeForWindow && darkMode)
                pAllowDarkModeForWindow(hYear, true);
            if (pSetWindowTheme && darkMode)
                pSetWindowTheme(hYear, L"CFD", nullptr);
        }
        do_conversion(hwnd, mode);
        return 0;
    }
    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        HMODULE hDwm = LoadLibraryA("dwmapi.dll");
        // Quick XP detection via dwmapi, use Window color instead of transparent color key
        HBRUSH brush = CreateSolidBrush(hDwm ? APP_LWA_COLORKEY : GetSysColor(COLOR_BTNFACE));
        if (hDwm)
            FreeLibrary(hDwm);
        FillRect(reinterpret_cast<HDC>(wparam), &rc, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_COMMAND:
    {
        // const WORD id = LOWORD(wparam);
        const WORD code = HIWORD(wparam);
        if (code == CBN_SELCHANGE)
        {
            do_conversion(hwnd, mode);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

constexpr static const wchar_t *converterClassName = L"ConverterDialog";

static void open_converter_dialog(converter_mode_t mode)
{
    UINT dpi = GetSystemDpi();
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_COMPOSITED | WS_EX_LAYERED,
        converterClassName,
        L"",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        dp(dpi, 4 * padding + padding / 2 + 3 * combo_width),
        dp(dpi, combo_height + 2 * padding),
        nullptr, nullptr, hInst, nullptr);
    SetLayeredWindowAttributes(hwnd, APP_LWA_COLORKEY, 0, LWA_COLORKEY);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(mode));
    if (hwnd)
        ShowWindow(hwnd, SW_SHOW);
}

static void update(HWND hwnd, app_state_t *state)
{
    unsigned days = today_in_days();
    persian_date_t date = days_to_persian(days);
    format_date(state->local_digits, PERSIAN, date, days % 7,
                state->notify_icon_data->szTip,
                sizeof(state->notify_icon_data->szTip) / sizeof(wchar_t));

    // szTip allocated string is both used for the tooltip and first item of the menu
    create_menu(state, state->notify_icon_data->szTip);

    HDC hdc = GetDC(hwnd);
    HICON icon = create_text_icon(hdc, format_number(date.day).value, state->black_background);
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
        if (lparam == WM_LBUTTONUP || lparam == WM_RBUTTONUP)
        {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(state->menu, TPM_RIGHTALIGN | TPM_RIGHTBUTTON | TPM_LAYOUTRTL,
                           p.x, p.y, 0, hwnd, nullptr);
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
        else if (wparam == converter_from_persian_id)
        {
            open_converter_dialog(PERSIAN);
            return 0;
        }
        else if (wparam == converter_from_gregorian_id)
        {
            open_converter_dialog(GREGORIAN);
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

static void enable_hidpi()
{
    using func_t = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
    auto pSetProcessDpiAwarenessContext = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext")));
    if (pSetProcessDpiAwarenessContext)
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

static void enable_dark_mode_support()
{
    using func_t = INT(WINAPI *)(INT); // undocumented SetPreferredAppMode's signature
    auto pSetPreferredAppMode = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(GetModuleHandleA("uxtheme.dll"), MAKEINTRESOURCEA(135))));
    if (pSetPreferredAppMode)
        pSetPreferredAppMode(/*Allow dark*/ 1);
}

// https://stackoverflow.com/a/10444161
// This is instead of putting a manifest XML
static ULONG_PTR enable_visual_styles()
{
    wchar_t dir[MAX_PATH];
    ULONG_PTR ulpActivationCookie = FALSE;
    ACTCTXW actCtx;
    SecureZeroMemory(&actCtx, sizeof(actCtx));
    actCtx.cbSize = sizeof(actCtx);
    actCtx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_SET_PROCESS_DEFAULT | ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actCtx.lpSource = L"shell32.dll";
    actCtx.lpAssemblyDirectory = dir;
    actCtx.lpResourceName = reinterpret_cast<LPCWSTR>(124);
    constexpr unsigned dir_size = sizeof(dir) / sizeof(*dir);
    UINT cch = GetSystemDirectoryW(dir, dir_size);
    if (cch >= dir_size)
        return FALSE; /*shouldn't happen*/
    dir[cch % dir_size] = L'\0';
    ActivateActCtx(CreateActCtxW(&actCtx), &ulpActivationCookie);
    return ulpActivationCookie;
}

extern "C" [[noreturn]] void start();
void start()
{
    HANDLE mutex = CreateMutexA(nullptr, 0, const_cast<char *>(appId));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(EXIT_FAILURE);

    enable_visual_styles();

    // Converter Dialog's class
    {
        WNDCLASSEXW wc;
        SecureZeroMemory(&wc, sizeof(WNDCLASSEXW));
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = ConverterDlgProc;
        wc.hInstance = hInst;
        wc.lpszClassName = converterClassName;
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
    }

    // Passing "STATIC" as a class name and overriding its Window procedure is a hack to avoid
    // registering a window class, which would require more code. The created window is never shown, so it doesn't
    // matter that it's a "STATIC" control.
    HWND hwnd = CreateWindowExA(0, "STATIC", nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));

    // Initiation
    NOTIFYICONDATAW notify_icon_data;
    SecureZeroMemory(&notify_icon_data, sizeof(NOTIFYICONDATAW));
    app_state_t state(&notify_icon_data);
    {
        enable_hidpi();
        enable_dark_mode_support();
        enable_visual_styles();
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

    // open_converter_dialog(PERSIAN);
    // open_converter_dialog(GREGORIAN);

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

    ExitProcess(msg.wParam);
}
