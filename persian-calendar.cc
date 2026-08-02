#include "warnings.hpp"
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define WINVER 0x0500 // XP support, and maybe 2000? Why not
IB_WARNING_PUSH
IB_WARNING_DISABLE_CLANG("-Wnonportable-system-include-path")
#include <windows.h>
IB_WARNING_POP
#include <shellapi.h>
#include <dwmapi.h>

#include "persian-calendar.h"

IB_WARNING_PUSH
IB_WARNING_DISABLE_CLANG("-Wreserved-identifier")
// https://web.archive.org/web/20190205041452/https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483
extern "C" IMAGE_DOS_HEADER __ImageBase;
IB_WARNING_POP
#define hInst (reinterpret_cast<HMODULE>(&__ImageBase))

template <typename T>
void zero_memory(T &ptr, size_t size = sizeof(T))
{
    SecureZeroMemory(&ptr, size);
}

static HFONT get_system_font(LONG size, bool disable_antialiasing = false)
{
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
    {
        ncm.lfMessageFont.lfHeight = size;
        if (disable_antialiasing)
            ncm.lfMessageFont.lfQuality = NONANTIALIASED_QUALITY;
        return CreateFontIndirectW(&ncm.lfMessageFont);
    }
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

struct LibraryLoader
{
private:
    HMODULE m_module;

    LibraryLoader(const LibraryLoader &) = delete;
    void operator=(const LibraryLoader &) = delete;

    static HMODULE getModuleWithFallback(const char *name)
    {
        HMODULE module = GetModuleHandleA(name);
        return module ? module : LoadLibraryA(name);
    }

public:
    LibraryLoader(const char *name) : m_module(getModuleWithFallback(name)) {}
    // Let's just don't free, all we load is system libraries and they are always loaded anyway, so no need to free them.
    // ~LibraryLoader() { if (module) FreeLibrary(module); }

    template <typename T>
    auto getProcedure(const char *procName)
    {
        return reinterpret_cast<T>(reinterpret_cast<void *>(GetProcAddress(m_module, procName)));
    }
};

static DWORD get_build_number()
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

static const char* get_wine_version()
{
    auto pWineGetVersion = LibraryLoader("ntdll.dll").getProcedure<const char*(CDECL *)()>("wine_get_version");
    return pWineGetVersion ? pWineGetVersion() : nullptr;
}

static bool is_dark_mode_active()
{
    // https://github.com/hrydgard/ppsspp/blob/10c2f05/Windows/W32Util/DarkMode.h#L68-L81
    if (get_build_number() < 17763)
        return false;
    auto pShouldAppsUseDarkMode = LibraryLoader("uxtheme.dll").getProcedure<bool(WINAPI *)()>(MAKEINTRESOURCEA(132)); // undocumented ShouldAppsUseDarkMode
    return pShouldAppsUseDarkMode && pShouldAppsUseDarkMode();
}

static bool is_system_in_dark_mode()
{
    if (get_build_number() < 18362)
        return is_dark_mode_active();
    auto pShouldSystemUseDarkMode = LibraryLoader("uxtheme.dll").getProcedure<bool(WINAPI *)()>(MAKEINTRESOURCEA(138)); // undocumented ShouldAppsUseDarkMode
    return pShouldSystemUseDarkMode && pShouldSystemUseDarkMode();
}

static HICON create_text_icon(HDC hdc, const wchar_t *text, bool black_background)
{
    const int size = 128; // GetSystemMetrics(SM_CXSMICON); oversized icon looks better
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hbmMask = CreateCompatibleBitmap(hdc, size, size);

    HDC memDC = CreateCompatibleDC(hdc);
    HGDIOBJ oldBmp = SelectObject(memDC, hbmColor);
    RECT rc{0, -14 /*tweak y height*/, size, size};

    FillRect(memDC, &rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, (black_background || is_system_in_dark_mode() ||
                         // on older systems always assume the task bar has dark colors and foreground should be white
                         get_build_number() < 18362 || get_wine_version())
                            ? RGB(255, 255, 255)
                            : RGB(0, 0, 0));

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

constexpr static const int mainTimerId = 1;
constexpr static const int widgetTimerId = 2;
constexpr static const wchar_t *widgetClassName = L"WgtDlg";
constexpr static const wchar_t *converterClassName = L"CnvDlg";

static UINT get_system_dpi()
{
    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return 96;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return static_cast<UINT>(dpi);
}

struct app_state_t
{
    NOTIFYICONDATAW *notify_icon_data;
    BOOL local_digits;
    BOOL black_background;
    HMENU menu;
    BOOL show_widget;
    BOOL fixed_widget_placement;
    BOOL always_on_top_widget;
    HWND widget_hwnd;

    app_state_t(NOTIFYICONDATAW *notify_icon_data_) : notify_icon_data(notify_icon_data_), local_digits(true),
                                                      black_background(get_build_number() < 18362), menu(nullptr),
                                                      show_widget(false), fixed_widget_placement(false),
                                                      always_on_top_widget(false), widget_hwnd(nullptr)
    {
    }
};

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
        menu_item.dwTypeData = const_cast<wchar_t *>(L"پس‌زمینهٔ سیاه آیکون");
        InsertMenuItemW(menu, black_background_id, TRUE, &menu_item);
    }
    InsertMenuW(menu, second_separator_id, MF_SEPARATOR, TRUE, nullptr);
    {
        menu_item.fState = state->show_widget ? MFS_CHECKED : 0;
        menu_item.wID = show_widget_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"نمایش ویجت");
        InsertMenuItemW(menu, show_widget_id, TRUE, &menu_item);
    }
    {
        menu_item.fState = state->fixed_widget_placement ? MFS_CHECKED : 0;
        menu_item.wID = fixed_widget_placement_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"مکان ثابت ویجت");
        InsertMenuItemW(menu, fixed_widget_placement_id, TRUE, &menu_item);
    }
    {
        menu_item.fState = state->always_on_top_widget ? MFS_CHECKED : 0;
        menu_item.wID = always_on_top_widget_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"ویجت همیشه نمایان");
        InsertMenuItemW(menu, always_on_top_widget_id, TRUE, &menu_item);
    }
    InsertMenuW(menu, third_separator_id, MF_SEPARATOR, TRUE, nullptr);
    {
        menu_item.fState = 0;
        menu_item.wID = date_converter_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"تبدیل تاریخ");
        InsertMenuItemW(menu, date_converter_id, TRUE, &menu_item);
    }
    InsertMenuW(menu, fourth_separator_id, MF_SEPARATOR, TRUE, nullptr);
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

template <typename T, size_t N>
constexpr size_t array_length(T (&)[N])
{
    return N;
}

struct formatted_number_t
{
    wchar_t value[8];
};
static formatted_number_t format_number(unsigned number, BOOL local_digits = true)
{
    formatted_number_t result;
    constexpr size_t size = array_length(result.value);
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

static void enable_help_button(HWND hWnd, bool enable)
{
    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (enable)
        exStyle |= WS_EX_CONTEXTHELP;
    else
        exStyle &= ~WS_EX_CONTEXTHELP;
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);
}

enum class update_source_t
{
    INIT,
    PERSIAN,
    GREGORIAN
};

struct date_combo_controller_t
{
private:
    HWND hDay, hMonth, hYear;

    unsigned base_year() const
    {
        return static_cast<unsigned>(GetWindowLongPtrW(hYear, GWLP_USERDATA));
    }

public:
    date_combo_controller_t(
        HWND hwnd, bool is_persian) : hDay(GetDlgItem(hwnd, static_cast<int>(is_persian ? dlg_persian_day_combo_id : dlg_gregorian_day_combo_id))),
                                      hMonth(GetDlgItem(hwnd, static_cast<int>(is_persian ? dlg_persian_month_combo_id : dlg_gregorian_month_combo_id))),
                                      hYear(GetDlgItem(hwnd, static_cast<int>(is_persian ? dlg_persian_year_combo_id : dlg_gregorian_year_combo_id))) {}

    date_t to_date() const
    {
        return {
            static_cast<unsigned>(SendMessageW(hYear, CB_GETCURSEL, 0, 0)) + base_year(),
            static_cast<unsigned>(SendMessageW(hMonth, CB_GETCURSEL, 0, 0)) + 1,
            static_cast<unsigned>(SendMessageW(hDay, CB_GETCURSEL, 0, 0)) + 1};
    }

    void set_from_date_triplet(const date_t &date)
    {
        SendMessageW(hYear, CB_SETCURSEL, date.year - base_year(), 0);
        SendMessageW(hMonth, CB_SETCURSEL, date.month - 1, 0);
        SendMessageW(hDay, CB_SETCURSEL, date.day - 1, 0);
    }
};

static void update_values(HWND hwnd, update_source_t source)
{
    date_combo_controller_t persian_combo(hwnd, true);
    date_combo_controller_t gregorian_combo(hwnd, false);

    unsigned days;
    if (source == update_source_t::INIT)
        days = today_in_days();
    else
        days = source == update_source_t::PERSIAN
                   ? persian_to_days(persian_combo.to_date())
                   : gregorian_to_days(gregorian_combo.to_date());

    persian_combo.set_from_date_triplet(days_to_persian(days));
    gregorian_combo.set_from_date_triplet(days_to_gregorian(days));

    unsigned today_days = today_in_days();
    const wchar_t *weekday = weekdays[(days + 3) % 7];
    wchar_t result[128];
    enable_help_button(hwnd, days != today_days);
    if (days == today_days)
        wsprintfW(result, L"%s، امروز", weekday);
    else if (days < today_days)
        wsprintfW(result, L"%s، %s روز پیش", weekday, format_number(today_days - days).value);
    else if (days > today_days)
        wsprintfW(result, L"%s، %s روز آتی", weekday, format_number(days - today_days).value);
    SetWindowTextW(hwnd, result);
}

constexpr int window_width = 6;
constexpr int window_height = 4;
constexpr int table_height_ratio = 2;

// In remembrance of old era Windows color/chroma keying,
// * https://devblogs.microsoft.com/oldnewthing/20251014-00/?p=111681
// * https://learn.microsoft.com/en-us/windows/win32/directshow/overlay-mixer-filter#:~:text=magenta%20for%20older%20256%2Dcolor%20cards
// Derived from the original magenta color to solve click-through issues
#define APP_LWA_COLORKEY (RGB(0xFE, 0x01, 0xFD))

static void update_layout(HWND hwnd, unsigned width, unsigned height)
{
    HFONT hFont = get_system_font(MulDiv(static_cast<int>(height), 8, 25 * table_height_ratio));
    for (unsigned i = 0; i < 6; ++i)
    {
        HWND item = GetDlgItem(hwnd, static_cast<int>(dlg_persian_day_combo_id + i));
        SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        unsigned row = i % 3;
        MoveWindow(item,
                   MulDiv(static_cast<int>(width), static_cast<int>(row == 0 ? 1 : (row == 1 ? 6 : 19)), 25),
                   MulDiv(static_cast<int>(height), static_cast<int>(i < 3 ? 2 : 14), 25 * table_height_ratio),
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
        auto pSetWindowTheme = LibraryLoader("uxtheme.dll").getProcedure<HRESULT(WINAPI *)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)>("SetWindowTheme");
        auto pGetComboBoxInfo = LibraryLoader("user32.dll").getProcedure<BOOL(WINAPI *)(HWND hwndCombo, PCOMBOBOXINFO pcbi)>("GetComboBoxInfo");
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

    LibraryLoader dwmapi("dwmapi.dll");
    {
        auto pDwmExtendFrameIntoClientArea = dwmapi.getProcedure<HRESULT(WINAPI *)(HWND, const MARGINS *)>(
            "DwmExtendFrameIntoClientArea");
        if (pDwmExtendFrameIntoClientArea)
        {
            MARGINS margins = {-1, -1, -1, -1};
            pDwmExtendFrameIntoClientArea(hwnd, &margins);
        }
    }
    {
        auto pDwmSetWindowAttribute = dwmapi.getProcedure<HRESULT(WINAPI *)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)>(
            "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute)
        {
            pDwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
            int backdropType = DWMSBT_TRANSIENTWINDOW; // instead of Mica's DWMSBT_MAINWINDOW
            pDwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
        }
    }
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

        if (RegQueryValueExW(key, show_widget_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            state->show_widget = !!value;

        if (RegQueryValueExW(key, fixed_widget_placement_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            state->fixed_widget_placement = !!value;

        if (RegQueryValueExW(key, always_on_top_widget_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            state->always_on_top_widget = !!value;
    }

    void set_local_digits(bool value) const
    {
        set_value(local_digits_key, value);
    }

    void set_black_background(bool value) const
    {
        set_value(black_background_key, value);
    }

    void set_show_widget(bool value) const
    {
        set_value(show_widget_key, value);
    }

    void set_fixed_widget_placement(bool value) const
    {
        set_value(fixed_widget_placement_key, value);
    }

    void set_always_on_top_widget(bool value) const
    {
        set_value(always_on_top_widget_key, value);
    }

    void set_widget_position(int left, int top, int size) const
    {
        set_value(widget_left_key, static_cast<DWORD>(left));
        set_value(widget_top_key, static_cast<DWORD>(top));
        set_value(widget_size_key, static_cast<DWORD>(size));
    }

    void get_widget_position(int &left, int &top, int &size) const
    {
        if (!key)
            return;
        DWORD value = 0;
        DWORD value_size = sizeof(DWORD);
        DWORD type = 0;

        if (RegQueryValueExW(key, widget_left_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &value_size) == ERROR_SUCCESS && type == REG_DWORD)
            left = static_cast<int>(value);
        if (RegQueryValueExW(key, widget_top_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &value_size) == ERROR_SUCCESS && type == REG_DWORD)
            top = static_cast<int>(value);
        if (RegQueryValueExW(key, widget_size_key, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &value_size) == ERROR_SUCCESS && type == REG_DWORD)
            size = static_cast<int>(value);
    }

    ~Registry()
    {
        if (key)
            RegCloseKey(key);
    }

private:
    HKEY key;

    void set_value(const wchar_t *name, DWORD value) const
    {
        if (!key)
            return;
        RegSetValueExW(
            key,
            name,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE *>(&value),
            sizeof(DWORD));
    }

    constexpr static const wchar_t *local_digits_key = L"LocalDigits";
    constexpr static const wchar_t *black_background_key = L"BlackBackground";
    constexpr static const wchar_t *show_widget_key = L"ShowWidget";
    constexpr static const wchar_t *widget_left_key = L"WidgetLeft";
    constexpr static const wchar_t *widget_top_key = L"WidgetTop";
    constexpr static const wchar_t *widget_size_key = L"WidgetSize";
    constexpr static const wchar_t *fixed_widget_placement_key = L"FixedWidget";
    constexpr static const wchar_t *always_on_top_widget_key = L"AlwaysOnTopWidget";
};

static void handle_widget_movability(HWND hwnd, app_state_t *app_state)
{
    if (hwnd == nullptr)
        return;
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (app_state->fixed_widget_placement)
        exStyle |= WS_EX_TRANSPARENT;
    else
        exStyle &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
}

static void handle_widget_always_on_top(HWND hwnd, app_state_t *app_state)
{
    if (hwnd == nullptr)
        return;
    SetWindowPos(hwnd, app_state->always_on_top_widget ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

static void handle_widget(HWND hwnd, app_state_t *app_state)
{
    HWND widgetHwnd = app_state->widget_hwnd;
    if (app_state->show_widget)
    {
        if (!widgetHwnd)
        {
            int left = CW_USEDEFAULT, top = CW_USEDEFAULT, size = CW_USEDEFAULT;
            Registry().get_widget_position(left, top, size);
            if (left == CW_USEDEFAULT || top == CW_USEDEFAULT || size == CW_USEDEFAULT)
            {
                RECT rc;
                if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0))
                {
                    size = static_cast<int>(rc.right / 10);
                    left = rc.right - size - size / 10;
                    top = rc.bottom - size - size / 10;
                }
                else
                    size = static_cast<int>(get_system_dpi() / 7 * 12);
            }
            widgetHwnd = CreateWindowExW(
                WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                widgetClassName, L"",
                WS_POPUP | WS_OVERLAPPED | WS_SIZEBOX,
                left, top,
                size, size,
                hwnd, nullptr, hInst, nullptr);
            SetTimer(widgetHwnd, widgetTimerId, 60000, nullptr);
            {
                DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
                auto pDwmSetWindowAttribute = LibraryLoader("dwmapi.dll").getProcedure<HRESULT(WINAPI *)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)>("DwmSetWindowAttribute");
                if (pDwmSetWindowAttribute)
                    pDwmSetWindowAttribute(
                        widgetHwnd,
                        DWMWA_WINDOW_CORNER_PREFERENCE,
                        &preference,
                        sizeof(preference));
            }
        }
        handle_widget_movability(widgetHwnd, app_state);
        handle_widget_always_on_top(widgetHwnd, app_state);
        ShowWindow(widgetHwnd, SW_SHOW);
        SetForegroundWindow(widgetHwnd);
        app_state->widget_hwnd = widgetHwnd;
    }
    else
    {
        if (widgetHwnd)
        {
            KillTimer(widgetHwnd, widgetTimerId);
            CloseWindow(widgetHwnd);
            DestroyWindow(widgetHwnd);
            // widgetHwnd = nullptr;
            app_state->widget_hwnd = nullptr;
        }
    }
}

static unsigned get_month_days(unsigned year, unsigned month)
{
    return persian_to_days({month == 12 ? year + 1 : year, month == 12 ? 1 : month + 1, 1}) - persian_to_days({year, month, 1});
}

static bool has_aero()
{
    return get_build_number() >= 4015 && !get_wine_version(); // https://betawiki.net/wiki/Windows_Aero
}

static void draw_table(
    const unsigned table_start, const unsigned table_top, const unsigned cell_size, HDC hdc, persian_date_t date,
    bool is_dark_mode, bool is_in_widget)
{
    HFONT hFont = get_system_font(static_cast<long>(cell_size), !is_in_widget);
    HGDIOBJ oldFont = SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, is_dark_mode ? RGB(255, 255, 255) : RGB(0, 0, 0));
    for (unsigned i = 0; i < 7; ++i)
    {
        RECT cell_rc{
            static_cast<long>(table_start + cell_size * i),
            static_cast<long>(table_top),
            static_cast<long>(table_start + cell_size * (i + 1)),
            static_cast<long>(table_top + cell_size)};
        DrawTextW(hdc, weekdays[i % 7], 1, &cell_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    unsigned week_start = (persian_to_days({date.year, date.month, 1}) + 3) % 7;
    unsigned month_days = get_month_days(date.year, date.month);
    bool has_aero = ::has_aero();
    for (unsigned i = week_start; i < month_days + week_start; ++i)
    {
        if (date.day == i + 1 - week_start)
            SetTextColor(hdc, is_dark_mode ? RGB(255, 255, 255) : RGB(0, 0, 0));
        else if (is_in_widget || !has_aero)
            SetTextColor(hdc, is_dark_mode ? RGB(160, 160, 160) : RGB(180, 180, 180));
        else
            SetTextColor(hdc, is_dark_mode ? RGB(160, 160, 160) : RGB(240, 240, 240));
        RECT cell_rc{
            static_cast<long>(table_start + cell_size * (i % 7)),
            static_cast<long>(table_top + cell_size * (i / 7 + 1)),
            static_cast<long>(table_start + cell_size * (i % 7 + 1)),
            static_cast<long>(table_top + cell_size * (i / 7 + 2))};
        DrawTextW(hdc, format_number(i + 1 - week_start).value, -1, &cell_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    HFONT hFont2 = get_system_font(static_cast<long>(MulDiv(static_cast<int>(cell_size), 7, 10)), !is_in_widget);
    SelectObject(hdc, hFont2);
    DeleteObject(hFont);
    {
        SetTextColor(hdc, is_dark_mode ? RGB(255, 255, 255) : RGB(0, 0, 0));
        RECT cell_rc{
            static_cast<long>(table_start),
            static_cast<long>(table_top + cell_size * 6),
            static_cast<long>(table_start + cell_size * 7),
            static_cast<long>(table_top + cell_size * 7)};
        wchar_t buf[64];
        wsprintfW(buf, L"%s %s", persian_months[(date.month - 1) % 12], format_number(date.year).value);
        DrawTextW(hdc, buf, -1, &cell_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, oldFont);
    DeleteObject(hFont2);
}

static LRESULT CALLBACK widget_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(hwnd, &wp))
            Registry().set_widget_position(wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right - wp.rcNormalPosition.left);
        break;
    }

    case WM_SIZE:
    {
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    }

    case WM_NCCALCSIZE:
    {
        if (wParam == TRUE)
            return 0;
        break;
    }

    case WM_NCHITTEST:
    {
        POINT pt{LOWORD(lParam), HIWORD(lParam)};
        ScreenToClient(hwnd, &pt);
        RECT rect;
        GetClientRect(hwnd, &rect);
        int borderZone = MulDiv(8, static_cast<int>(get_system_dpi()), 96);
        bool isLeft = pt.x > rect.right - borderZone; // Flipped for RTL
        bool isRight = pt.x < rect.left + borderZone; // Flipped for RTL
        bool isTop = pt.y < rect.top + borderZone;
        bool isBottom = pt.y > rect.bottom - borderZone;
        if (isTop && isLeft)
            return HTTOPLEFT;
        if (isTop && isRight)
            return HTTOPRIGHT;
        if (isBottom && isLeft)
            return HTBOTTOMLEFT;
        if (isBottom && isRight)
            return HTBOTTOMRIGHT;
        if (isLeft)
            return HTLEFT;
        if (isRight)
            return HTRIGHT;
        if (isTop)
            return HTTOP;
        if (isBottom)
            return HTBOTTOM;
        return HTCAPTION;
    }

    case WM_SIZING:
    {
        RECT *rect = reinterpret_cast<RECT *>(lParam);
        int currentWidth = rect->right - rect->left;
        int currentHeight = rect->bottom - rect->top;
        switch (wParam)
        {
        case WMSZ_RIGHT:
        case WMSZ_BOTTOMRIGHT:
            rect->bottom = rect->top + currentWidth;
            break;

        case WMSZ_BOTTOM:
            rect->right = rect->left + currentHeight;
            break;

        case WMSZ_LEFT:
        case WMSZ_BOTTOMLEFT:
            rect->bottom = rect->top + currentWidth;
            break;

        case WMSZ_TOP:
        case WMSZ_TOPRIGHT:
            rect->right = rect->left + currentHeight;
            break;

        case WMSZ_TOPLEFT:
            rect->top = rect->bottom - currentWidth;
            break;
        default:
            break;
        }
        return TRUE;
    }

    case WM_TIMER:
    case WM_SETTINGCHANGE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        const int table_size = static_cast<int>(ps.rcPaint.bottom - ps.rcPaint.top);
        const unsigned cell_size = static_cast<unsigned>(table_size / 8);
        const unsigned padding = (static_cast<unsigned>(table_size) - cell_size * 7) / 2;
        bool is_dark_mode = is_system_in_dark_mode();
        {
            HBRUSH background_brush = CreateSolidBrush(is_dark_mode ? RGB(32, 32, 32) : RGB(255, 255, 255));
            FillRect(hdc, &ps.rcPaint, background_brush);
            DeleteObject(background_brush);
        }
        draw_table(padding, padding, cell_size, hdc, days_to_persian(today_in_days()), is_dark_mode, true);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_NCACTIVATE:
        return TRUE;

    case WM_CREATE:
    {
        constexpr int default_window_alpha = 200;
        SetLayeredWindowAttributes(hwnd, 0, default_window_alpha, LWA_ALPHA);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        break;
    }

    // Make the whole window movable
    case WM_LBUTTONDOWN:
    {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK converter_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
            HWND item = CreateWindowExW(
                0, L"COMBOBOX", nullptr,
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
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        {
            HBRUSH brush = CreateSolidBrush(has_aero() ? APP_LWA_COLORKEY : GetSysColor(COLOR_BTNFACE));
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(brush);
        }

        {
            persian_date_t date = date_combo_controller_t(hwnd, true).to_date();
            const unsigned cell_size = static_cast<unsigned>(ps.rcPaint.bottom / 16);
            unsigned table_top = static_cast<unsigned>(ps.rcPaint.bottom / table_height_ratio);
            unsigned table_start = static_cast<unsigned>((ps.rcPaint.right - ps.rcPaint.left - static_cast<int>(cell_size) * 7) / 2);
            bool is_dark_mode = is_system_in_dark_mode();
            draw_table(table_start, table_top, cell_size, hdc, date, is_dark_mode, false);
        }

        EndPaint(hwnd, &ps);
        break;
    }

    // Handle help button
    case WM_NCLBUTTONDOWN:
    {
        if (wParam == HTHELP)
        {
            update_values(hwnd, update_source_t::INIT);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        bool darkMode = is_dark_mode_active();
        SetTextColor(hdc, darkMode ? RGB(240, 240, 240) : RGB(0, 0, 0));
        SetBkColor(hdc, darkMode ? RGB(32, 32, 32) : RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(GetStockObject(darkMode ? DKGRAY_BRUSH : WHITE_BRUSH));
    }

    case WM_SIZE:
    {
        unsigned newWidth = static_cast<unsigned>(LOWORD(lParam));
        unsigned newHeight = static_cast<unsigned>(HIWORD(lParam));
        update_layout(hwnd, newWidth, newHeight);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    // Make the whole window movable
    case WM_LBUTTONDOWN:
    {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }

    case WM_COMMAND:
    {
        const WORD id = LOWORD(wParam);
        const WORD code = HIWORD(wParam);
        if (code == CBN_SELCHANGE)
        {
            update_values(hwnd, id < dlg_gregorian_day_combo_id ? update_source_t::PERSIAN : update_source_t::GREGORIAN);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        // If hwnd doesn't have a parent, it means it's in the portable mode
        if (!GetWindow(hwnd, GW_OWNER))
        {
            PostQuitMessage(ERROR_SUCCESS);
            return 0;
        }
        break;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void open_converter_dialog(HWND parent)
{
    UINT dpi = get_system_dpi();
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_OVERLAPPEDWINDOW | WS_EX_TOPMOST | WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_COMPOSITED | (get_wine_version() ? 0 : WS_EX_LAYERED),
        converterClassName, L"",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        static_cast<int>(window_width * dpi),
        static_cast<int>(window_height * dpi),
        parent, nullptr, hInst, nullptr);
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

const unsigned notifyClickId = WM_USER + 1;
static LRESULT CALLBACK tray_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    app_state_t *state = reinterpret_cast<app_state_t *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(ERROR_SUCCESS);
        return 0;

    case WM_TIMER:
    case WM_SETTINGCHANGE:
        update(hwnd, state);
        return 0;

    case notifyClickId:
        if (lParam == WM_RBUTTONUP)
        {
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(state->menu, TPM_RIGHTALIGN | TPM_RIGHTBUTTON | TPM_LAYOUTRTL,
                           p.x, p.y, 0, hwnd, nullptr);
        }
        else if (lParam == WM_LBUTTONDBLCLK)
            open_converter_dialog(hwnd);
        return 0;

    case WM_COMMAND:
        if (wParam == local_digits_id)
        {
            bool newValue = !state->local_digits;
            state->local_digits = newValue;
            update(hwnd, state);
            Registry().set_local_digits(newValue);
            return 0;
        }
        else if (wParam == black_background_id)
        {
            bool newValue = !state->black_background;
            state->black_background = newValue;
            update(hwnd, state);
            Registry().set_black_background(newValue);
            return 0;
        }
        else if (wParam == show_widget_id)
        {
            bool newValue = !state->show_widget;
            state->show_widget = newValue;
            handle_widget(hwnd, state);
            const Registry &registry = Registry();
            // if (!newValue)
            //     registry.set_widget_position(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);
            update(hwnd, state);
            registry.set_show_widget(newValue);
            return 0;
        }
        else if (wParam == fixed_widget_placement_id)
        {
            bool newValue = !state->fixed_widget_placement;
            state->fixed_widget_placement = newValue;
            update(hwnd, state);
            handle_widget_movability(state->widget_hwnd, state);
            Registry().set_fixed_widget_placement(newValue);
            return 0;
        }
        else if (wParam == always_on_top_widget_id)
        {
            bool newValue = !state->always_on_top_widget;
            state->always_on_top_widget = newValue;
            update(hwnd, state);
            handle_widget_always_on_top(state->widget_hwnd, state);
            Registry().set_always_on_top_widget(newValue);
            return 0;
        }
        else if (wParam == date_converter_id)
        {
            open_converter_dialog(hwnd);
            return 0;
        }
        else if (wParam == exit_id)
        {
            PostQuitMessage(ERROR_SUCCESS);
            return 0;
        }
        break;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void enable_hidpi()
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

static void enable_dark_mode_support()
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

// https://stackoverflow.com/a/10444161
// This is instead of putting a manifest XML
static void enable_visual_styles()
{
    // CreateActCtxA and ActivateActCtx aren't available in Windowws 2000, so
    LibraryLoader kernel32("kernel32.dll");
    auto pCreateActCtxA = kernel32.getProcedure<HANDLE(WINAPI *)(PCACTCTXA pActCtx)>("CreateActCtxA");
    auto pActivateActCtx = kernel32.getProcedure<BOOL(WINAPI *)(HANDLE hActCtx, ULONG_PTR * lpCookie)>(
        "ActivateActCtx");
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

IB_WARNING_PUSH
IB_WARNING_DISABLE_CLANG("-Wunsafe-buffer-usage")
template <size_t N>
static bool has_string_suffix(const wchar_t *str, const wchar_t (&suffix)[N])
{
    int str_len = lstrlenW(str);
    constexpr int suffix_len = static_cast<int>(N) - 1;
    if (str_len < suffix_len)
        return false;
    const wchar_t *check_ptr = str + (str_len - suffix_len);
    for (size_t i = 0; i < suffix_len; ++i)
        if (check_ptr[i] != suffix[i])
            return false;
    return true;
}
IB_WARNING_POP

extern "C" [[noreturn]] void start();
void start()
{
    bool is_portable = has_string_suffix(GetCommandLineW(), L"/p");

    HANDLE mutex = CreateMutexW(nullptr, 0, appId);
    is_portable |= !mutex || GetLastError() == ERROR_ALREADY_EXISTS;

    {
        WNDCLASSEXW wc;
        zero_memory(wc);
        if (is_portable)
            wc.hIcon = LoadIconW(nullptr, IDI_ASTERISK);
        wc.hInstance = hInst;
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        // Tray Menu's class
        wc.lpfnWndProc = tray_window_procedure;
        wc.lpszClassName = appId;
        RegisterClassExW(&wc);
        // Converter Dialog's class
        wc.lpfnWndProc = converter_window_procedure;
        wc.lpszClassName = converterClassName;
        RegisterClassExW(&wc);
        // Widget Dialog's class
        wc.lpfnWndProc = widget_window_procedure;
        wc.lpszClassName = widgetClassName;
        RegisterClassExW(&wc);
    }
    HWND hwnd = is_portable ? nullptr : CreateWindowExW(0, appId, nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

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
        if (!is_portable)
            Shell_NotifyIconW(NIM_ADD, &notify_icon_data);
    }

    app_state_t state(&notify_icon_data);
    if (!is_portable)
    {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
        Registry().fill_app_state(&state);
        handle_widget(hwnd, &state);
        update(hwnd, &state);
        SetTimer(hwnd, mainTimerId, 60000, nullptr);
    }

    if (is_portable)
        open_converter_dialog(hwnd);

    // Main loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Finalize
    if (!is_portable)
    {
        Shell_NotifyIconW(NIM_DELETE, &notify_icon_data);
        DestroyIcon(notify_icon_data.hIcon);
    }

    ExitProcess(static_cast<UINT>(msg.wParam));
}
