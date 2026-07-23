#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define WINVER 0x0500 // XP support, and maybe 2000? Why not
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>

#include "persian-calendar.h"

// https://web.archive.org/web/20190205041452/https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483
extern "C" IMAGE_DOS_HEADER __ImageBase;
#define hInst (reinterpret_cast<HMODULE>(&__ImageBase))

template <typename T>
void zero_memory(T &ptr, size_t size = sizeof(T))
{
    SecureZeroMemory(&ptr, size);
}

static HFONT get_system_font(LONG size)
{
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
    {
        ncm.lfMessageFont.lfHeight = size;
        return CreateFontIndirectW(&ncm.lfMessageFont);
    }
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

static HICON create_text_icon(HDC hdc, const wchar_t *text, bool black_background)
{
    const int size = 128; // GetSystemMetrics(SM_CXSMICON); oversized icon looks better
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hbmMask = CreateCompatibleBitmap(hdc, size, size);

    HDC memDC = CreateCompatibleDC(hdc);
    HGDIOBJ oldBmp = SelectObject(memDC, hbmColor);
    RECT rc{0, 0, size, size};

    FillRect(memDC, &rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    HFONT hFont = get_system_font(-size + 12);
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

    ICONINFO iconInfo;
    zero_memory(iconInfo);
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
constexpr unsigned date_converter_id = 1005;
constexpr unsigned third_separator_id = 1006;
constexpr unsigned exit_id = 1007;
static void create_menu(app_state_t *state, wchar_t *date)
{
    HMENU menu = CreatePopupMenu();
    MENUITEMINFOW menu_item;
    zero_memory(menu_item);
    menu_item.cbSize = sizeof(MENUITEMINFOW);
    menu_item.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | MIIM_DATA;
    {
        menu_item.fState = MFS_DISABLED;
        menu_item.wID = date_id;
        menu_item.dwTypeData = date;
        InsertMenuItemW(menu, date_id, TRUE, &menu_item);
    }
    InsertMenuW(menu, first_separator_id, MF_SEPARATOR, TRUE, nullptr);
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
    InsertMenuW(menu, second_separator_id, MF_SEPARATOR, TRUE, nullptr);
    {
        menu_item.fState = 0;
        menu_item.wID = date_converter_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"تبدیل تاریخ");
        InsertMenuItemW(menu, date_converter_id, TRUE, &menu_item);
    }
    InsertMenuW(menu, third_separator_id, MF_SEPARATOR, TRUE, nullptr);
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
const static wchar_t *gregorian_months[] = {
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

constexpr unsigned dlg_persian_day_combo_id = 2001;
constexpr unsigned dlg_persian_month_combo_id = 2002;
constexpr unsigned dlg_persian_year_combo_id = 2003;
constexpr unsigned dlg_gregorian_day_combo_id = 2004;
constexpr unsigned dlg_gregorian_month_combo_id = 2005;
constexpr unsigned dlg_gregorian_year_combo_id = 2006;

struct formatted_number_t
{
    wchar_t value[8];
};
static formatted_number_t format_number(unsigned number, BOOL local_digits = true)
{
    formatted_number_t result;
    constexpr unsigned size = sizeof(result.value) / sizeof(wchar_t);
    wsprintfW(result.value, L"%d", number);
    if (local_digits)
        for (unsigned i = 0; i < size && result.value[i % size]; ++i)
            result.value[i % size] += L'۰' - L'0';
    return result;
}

static unsigned today_in_days()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return gregorian_to_days({static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth), static_cast<unsigned>(st.wDay)});
}

enum class update_source_t
{
    INIT,
    PERSIAN,
    GREGORIAN
};

static void update_values(HWND hwnd, update_source_t source)
{
    HWND hDayPersian = GetDlgItem(hwnd, dlg_persian_day_combo_id);
    HWND hMonthPersian = GetDlgItem(hwnd, dlg_persian_month_combo_id);
    HWND hYearPersian = GetDlgItem(hwnd, dlg_persian_year_combo_id);
    HWND hDayGregorian = GetDlgItem(hwnd, dlg_gregorian_day_combo_id);
    HWND hMonthGregorian = GetDlgItem(hwnd, dlg_gregorian_month_combo_id);
    HWND hYearGregorian = GetDlgItem(hwnd, dlg_gregorian_year_combo_id);

    unsigned persianBaseYear = static_cast<unsigned>(GetWindowLongPtrW(hYearPersian, GWLP_USERDATA));
    unsigned gregorianBaseYear = static_cast<unsigned>(GetWindowLongPtrW(hYearGregorian, GWLP_USERDATA));

    unsigned days;
    if (source == update_source_t::INIT)
        days = today_in_days();
    else
    {
        bool is_persian = source == update_source_t::PERSIAN;
        HWND sourceYear = is_persian ? hYearPersian : hYearGregorian;
        unsigned baseYear = is_persian ? persianBaseYear : gregorianBaseYear;
        date_triplet_t input_date{
            static_cast<unsigned>(SendMessageW(sourceYear, CB_GETCURSEL, 0, 0)) + baseYear,
            static_cast<unsigned>(SendMessageW(is_persian ? hMonthPersian : hMonthGregorian, CB_GETCURSEL, 0, 0)) + 1,
            static_cast<unsigned>(SendMessageW(is_persian ? hDayPersian : hDayGregorian, CB_GETCURSEL, 0, 0)) + 1};
        days = is_persian ? persian_to_days(input_date) : gregorian_to_days(input_date);
    }

    {
        persian_date_t date = days_to_persian(days);
        SendMessageW(hDayPersian, CB_SETCURSEL, date.day - 1, 0);
        SendMessageW(hMonthPersian, CB_SETCURSEL, date.month - 1, 0);
        SendMessageW(hYearPersian, CB_SETCURSEL, date.year - persianBaseYear, 0);
    }
    {
        gregorian_date_t date = days_to_gregorian(days);
        SendMessageW(hDayGregorian, CB_SETCURSEL, date.day - 1, 0);
        SendMessageW(hMonthGregorian, CB_SETCURSEL, date.month - 1, 0);
        SendMessageW(hYearGregorian, CB_SETCURSEL, date.year - gregorianBaseYear, 0);
    }

    unsigned today_days = today_in_days();
    const wchar_t *weekday = weekdays[(days + 3) % 7];
    wchar_t result[128];
    if (days == today_days)
        wsprintfW(result, L"%s، امروز", weekday);
    else if (days < today_days)
        wsprintfW(result, L"%s، %s روز پیش", weekday, format_number(today_days - days).value);
    else if (days > today_days)
        wsprintfW(result, L"%s، %s روز آتی", weekday, format_number(days - today_days).value);
    SetWindowTextW(hwnd, result);
}

static UINT get_system_dpi()
{
    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return 96;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return static_cast<UINT>(dpi);
}

constexpr int window_width = 6;
constexpr int window_height = 2;

template <typename T>
auto get_proc(HMODULE hModule, const char *procName)
{
    return reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(hModule, procName)));
}

static DWORD get_build_number()
{
    auto pRtlGetVersion = get_proc<LONG(WINAPI *)(PRTL_OSVERSIONINFOW lpVersionInformation)>(
        GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (pRtlGetVersion)
    {
        RTL_OSVERSIONINFOW rovi;
        rovi.dwOSVersionInfoSize = sizeof(rovi);
        if (pRtlGetVersion(&rovi) == 0)
            return rovi.dwBuildNumber;
    }
    return 0;
}

static bool is_dark_mode_active()
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    if (get_build_number() < 17763)
        return false;
    auto pShouldAppsUseDarkMode = get_proc<bool (WINAPI *)()>(
        GetModuleHandleA("uxtheme.dll"), MAKEINTRESOURCEA(132)); // undocumented ShouldAppsUseDarkMode
    return pShouldAppsUseDarkMode && pShouldAppsUseDarkMode();
}

// In remembrance of old era Windows color/chroma keying,
// * https://devblogs.microsoft.com/oldnewthing/20251014-00/?p=111681
// * https://learn.microsoft.com/en-us/windows/win32/directshow/overlay-mixer-filter#:~:text=magenta%20for%20older%20256%2Dcolor%20cards
// Derived from the original magenta color to solve click-through issues
#define APP_LWA_COLORKEY (RGB(0xFE, 0x01, 0xFD))

static void update_layout(HWND hwnd, unsigned width, unsigned height)
{
    HFONT hFont = get_system_font(MulDiv(static_cast<int>(height), 8, 25));
    for (unsigned i = 0; i < 6; ++i)
    {
        HWND item = GetDlgItem(hwnd, static_cast<int>(dlg_persian_day_combo_id + i));
        SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        int row = i % 3;
        MoveWindow(item,
                   MulDiv(static_cast<int>(width), static_cast<int>(row == 0 ? 1 : (row == 1 ? 6 : 19)), 25),
                   MulDiv(static_cast<int>(height), static_cast<int>(i < 3 ? 2 : 14), 25),
                   MulDiv(static_cast<int>(width), row == 0 ? 4 : (row == 1 ? 12 : 5), 25),
                   // The height parameter here is only used for the dropdown size of the ComboBox,
                   // so making it larger ensures the dropdown is sufficiently tall.
                   // Different versions of Windows seem to ignore it and only Wine considers it
                   static_cast<int>(5 * height),
                   TRUE);
    }
}

static void update_window_visual_styles(HWND hwnd)
{
    BOOL darkMode = is_dark_mode_active();

    {
        auto pSetWindowTheme = get_proc<HRESULT(WINAPI *)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)>(
            GetModuleHandleA("uxtheme.dll"), "SetWindowTheme");
        auto pGetComboBoxInfo = get_proc<BOOL(WINAPI *)(HWND hwndCombo, PCOMBOBOXINFO pcbi)>(
            GetModuleHandleA("user32.dll"), "GetComboBoxInfo");
        if (pSetWindowTheme)
            for (unsigned id = dlg_persian_day_combo_id; id <= dlg_gregorian_year_combo_id; ++id)
            {
                HWND item = GetDlgItem(hwnd, static_cast<int>(id));
                pSetWindowTheme(item, darkMode ? L"DarkMode_CFD" : L"Explorer", nullptr);
                if (pGetComboBoxInfo)
                {
                    COMBOBOXINFO cbi;
                    cbi.cbSize = sizeof(cbi);
                    if (pGetComboBoxInfo(item, &cbi))
                        pSetWindowTheme(cbi.hwndList, darkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
                }
            }
    }

    HMODULE hDwmapi = LoadLibraryA("dwmapi.dll");
    {
        auto pDwmExtendFrameIntoClientArea = get_proc<HRESULT(WINAPI *)(HWND, const MARGINS *)>(
            hDwmapi, "DwmExtendFrameIntoClientArea");
        if (pDwmExtendFrameIntoClientArea)
        {
            MARGINS margins = {-1, -1, -1, -1};
            pDwmExtendFrameIntoClientArea(hwnd, &margins);
        }
    }
    {
        auto pDwmSetWindowAttribute = get_proc<HRESULT(WINAPI *)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)>(
            hDwmapi, "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute)
        {
            pDwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
            int backdropType = DWMSBT_MAINWINDOW;
            pDwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
        }
    }
    if (hDwmapi)
        FreeLibrary(hDwmapi);
}

static LRESULT CALLBACK ConverterDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        unsigned days = today_in_days();
        persian_date_t persian_date = days_to_persian(days);
        gregorian_date_t gregorian_date = days_to_gregorian(days);

        for (unsigned i = 0; i < 6; ++i)
        {
            HWND item = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                        0, 0, 0, 0, hwnd,
                                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_persian_day_combo_id + i)), hInst, nullptr);
            bool is_persian = i < 3;
            unsigned row = i % 3;
            if (row == 0)
                for (unsigned j = 1; j <= 31; ++j)
                    SendMessageW(item, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format_number(j).value));
            else if (row == 1)
                for (unsigned j = 0; j < 12; ++j)
                {
                    wchar_t buf[32];
                    wsprintfW(buf, L"%s (%s)",
                              is_persian ? persian_months[j % 12] : gregorian_months[j % 12],
                              format_number(j + 1).value);
                    SendMessageW(item, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
                }
            else
            {
                constexpr unsigned combobox_years = 200;
                unsigned base_year = (is_persian ? persian_date.year : gregorian_date.year) - combobox_years / 2;
                SetWindowLongPtrW(item, GWLP_USERDATA, static_cast<LONG_PTR>(base_year));
                for (unsigned j = 0; j <= combobox_years; ++j)
                    SendMessageW(item, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(format_number(base_year + j).value));
            }
        }
        update_values(hwnd, update_source_t::INIT);
    }
        [[fallthrough]];
    case WM_SETTINGCHANGE:
        update_window_visual_styles(hwnd);
        return 0;

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = reinterpret_cast<HDC>(wparam);
        bool darkMode = is_dark_mode_active();
        SetTextColor(hdc, darkMode ? RGB(240, 240, 240) : RGB(0, 0, 0));
        SetBkColor(hdc, darkMode ? RGB(32, 32, 32) : RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(GetStockObject(darkMode ? DKGRAY_BRUSH : WHITE_BRUSH));
    }

    case WM_SIZE:
    {
        unsigned newWidth = static_cast<unsigned>(LOWORD(lparam));
        unsigned newHeight = static_cast<unsigned>(HIWORD(lparam));
        update_layout(hwnd, newWidth, newHeight);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        bool has_aero = get_build_number() >= 4015; // https://betawiki.net/wiki/Windows_Aero
        HBRUSH brush = CreateSolidBrush(has_aero ? APP_LWA_COLORKEY : GetSysColor(COLOR_BTNFACE));
        FillRect(reinterpret_cast<HDC>(wparam), &rc, brush);
        DeleteObject(brush);
        return 1;
    }

    case WM_COMMAND:
    {
        const WORD id = LOWORD(wparam);
        const WORD code = HIWORD(wparam);
        if (code == CBN_SELCHANGE)
        {
            update_values(hwnd, id < dlg_gregorian_day_combo_id ? update_source_t::PERSIAN : update_source_t::GREGORIAN);
            return 0;
        }
        break;
    }

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

constexpr static const wchar_t *converterClassName = L"CnvDlg";

static void open_converter_dialog()
{
    UINT dpi = get_system_dpi();
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_COMPOSITED | WS_EX_LAYERED,
        converterClassName,
        L"",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        static_cast<int>(window_width * dpi),
        static_cast<int>(window_height * dpi),
        nullptr, nullptr, hInst, nullptr);
    SetLayeredWindowAttributes(hwnd, APP_LWA_COLORKEY, 0, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

static void update(HWND hwnd, app_state_t *state)
{
    unsigned days = today_in_days();
    persian_date_t date = days_to_persian(days);
    BOOL local_digits = state->local_digits;
    NOTIFYICONDATAW &nid = *state->notify_icon_data;
    wsprintfW(nid.szTip,
              L"%s، %s %s(%s) %s",
              weekdays[(days + 3) % 7],
              format_number(date.day, local_digits).value,
              persian_months[(date.month - 1) % 12],
              format_number(date.month, local_digits).value,
              format_number(date.year, local_digits).value);

    // szTip allocated string is both used for the tooltip and first item of the menu
    create_menu(state, nid.szTip);

    HDC hdc = GetDC(hwnd);
    HICON icon = create_text_icon(hdc, format_number(date.day, local_digits).value, state->black_background);
    ReleaseDC(hwnd, hdc);
    if (nid.hIcon)
        DestroyIcon(nid.hIcon);
    nid.hIcon = icon;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

#define appId L"PersianCalendarWin32"
struct Registry
{
    Registry(const Registry &) = delete;
    void operator=(const Registry &) = delete;
    Registry() : key(nullptr)
    {
        LONG status = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\" appId,
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

        if (RegQueryValueExW(key, local_digits_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            state->local_digits = !!value;

        if (RegQueryValueExW(key, black_background_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
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

    void set_bool(const wchar_t *name, bool value) const
    {
        if (!key)
            return;
        DWORD dword = value ? 1 : 0;
        RegSetValueExW(
            key,
            name,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE *>(&dword),
            sizeof(DWORD));
    }

    constexpr static const wchar_t *local_digits_key = L"LocalDigits";
    constexpr static const wchar_t *black_background_key = L"BlackBackground";
};

const unsigned notifyClickId = WM_USER + 1;
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    app_state_t *state = reinterpret_cast<app_state_t *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(ERROR_SUCCESS);
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
            open_converter_dialog();
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
        else if (wparam == date_converter_id)
        {
            open_converter_dialog();
            return 0;
        }
        else if (wparam == exit_id)
        {
            PostQuitMessage(ERROR_SUCCESS);
            return 0;
        }
        break;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void enable_hidpi()
{
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    auto pSetProcessDpiAwarenessContext = get_proc<BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT value)>(
        hUser32, "SetProcessDpiAwarenessContext");
    if (pSetProcessDpiAwarenessContext)
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else
    {
        auto pSetProcessDPIAware = get_proc<BOOL(WINAPI *)()>(hUser32, "SetProcessDPIAware");
        if (pSetProcessDPIAware)
            pSetProcessDPIAware();
    }
}

static void enable_dark_mode_support()
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    HMODULE hUxTheme = GetModuleHandleA("uxtheme.dll");
    if (get_build_number() < 18362)
    {
        auto pAllowDarkModeForApp = get_proc<bool (WINAPI *)(bool allow)>(
            hUxTheme, MAKEINTRESOURCEA(135)); // undocumented AllowDarkModeForApp
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
        auto pSetPreferredAppMode = get_proc<INT(WINAPI *)(PreferredAppMode value)>(
            hUxTheme, MAKEINTRESOURCEA(135)); // undocumented SetPreferredAppMode
        if (pSetPreferredAppMode)
            pSetPreferredAppMode(PreferredAppMode::AllowDark);
    }
}

// https://stackoverflow.com/a/10444161
// This is instead of putting a manifest XML
static void enable_visual_styles()
{
    // CreateActCtxA and ActivateActCtx aren't available in Windowws 2000, so
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    auto pCreateActCtxA = get_proc<HANDLE(WINAPI *)(PCACTCTXA pActCtx)>(hKernel32, "CreateActCtxA");
    auto pActivateActCtx = get_proc<BOOL(WINAPI *)(HANDLE hActCtx, ULONG_PTR * lpCookie)>(
        hKernel32, "ActivateActCtx");
    if (!pCreateActCtxA || !pActivateActCtx)
        return;

    char dir[MAX_PATH];
    GetSystemDirectoryA(dir, MAX_PATH);
    ACTCTXA actCtx;
    zero_memory(actCtx);
    actCtx.cbSize = sizeof(actCtx);
    actCtx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_SET_PROCESS_DEFAULT | ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actCtx.lpSource = "shell32.dll";
    actCtx.lpAssemblyDirectory = dir;
    actCtx.lpResourceName = MAKEINTRESOURCEA(124);
    ULONG_PTR ulpActivationCookie = FALSE;
    pActivateActCtx(pCreateActCtxA(&actCtx), &ulpActivationCookie);
}

extern "C" [[noreturn]] void start();
void start()
{
    HANDLE mutex = CreateMutexW(nullptr, 0, appId);
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(1);

    // Converter Dialog's class
    {
        WNDCLASSEXW wc;
        zero_memory(wc);
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = ConverterDlgProc;
        wc.hInstance = hInst;
        wc.lpszClassName = converterClassName;
        if (!RegisterClassExW(&wc))
            ExitProcess(1);
    }

    // Tray Menu's class
    {
        WNDCLASSEXW wc;
        zero_memory(wc);
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.hInstance = hInst;
        wc.lpfnWndProc = WndProc;
        wc.lpszClassName = appId;
        if (!RegisterClassExW(&wc))
            ExitProcess(1);
    }
    HWND hwnd = CreateWindowExW(0, appId, nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

    enable_visual_styles();
    enable_hidpi();
    enable_dark_mode_support();

    // Initiation
    NOTIFYICONDATAW notify_icon_data;
    {
        zero_memory(notify_icon_data);
        notify_icon_data.cbSize = sizeof(NOTIFYICONDATAW);
        notify_icon_data.uCallbackMessage = notifyClickId;
        notify_icon_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        notify_icon_data.hWnd = hwnd;
        Shell_NotifyIconW(NIM_ADD, &notify_icon_data);
    }

    app_state_t state(&notify_icon_data);
    {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
        Registry().fill_app_state(&state);
        update(hwnd, &state);
        SetTimer(hwnd, 1 /*timer id*/, 60000, nullptr);
    }

    // open_converter_dialog();

    // Main loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Finalize
    {
        Shell_NotifyIconW(NIM_DELETE, &notify_icon_data);
        DestroyIcon(notify_icon_data.hIcon);
    }

    ExitProcess(msg.wParam);
}
