#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
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

// https://web.archive.org/web/20190205041452/https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483
extern "C" IMAGE_DOS_HEADER __ImageBase;

constexpr unsigned date_id = 1000;
constexpr unsigned first_separator_id = 1001;
constexpr unsigned local_digits_id = 1002;
constexpr unsigned black_background_id = 1003;
constexpr unsigned second_separator_id = 1004;
constexpr unsigned exit_id = 1005;
constexpr unsigned convert_id = 1006;
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
        menu_item.wID = convert_id;
        menu_item.dwTypeData = const_cast<wchar_t *>(L"تبدیل تاریخ");
        InsertMenuItemW(menu, convert_id, TRUE, &menu_item);
    }
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

static const char *gregorian_months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};

constexpr unsigned dlg_cal_combo_id   = 2001;
constexpr unsigned dlg_day_combo_id   = 2002;
constexpr unsigned dlg_mon_combo_id   = 2003;
constexpr unsigned dlg_year_edit_id   = 2004;
constexpr unsigned dlg_convert_btn_id = 2005;
constexpr unsigned dlg_result_lbl_id  = 2006;

static void do_conversion(HWND hwnd)
{
    int cal = static_cast<int>(SendDlgItemMessageW(hwnd, dlg_cal_combo_id, CB_GETCURSEL, 0, 0));
    BOOL ok;
    unsigned day  = GetDlgItemInt(hwnd, dlg_day_combo_id, &ok, FALSE);
    unsigned mon  = GetDlgItemInt(hwnd, dlg_mon_combo_id, &ok, FALSE);
    unsigned year = GetDlgItemInt(hwnd, dlg_year_edit_id, &ok, FALSE);
    HWND hResult  = GetDlgItem(hwnd, dlg_result_lbl_id);
    if (day == 0 || day > 31 || mon == 0 || mon > 12 || year == 0)
    {
        SetWindowTextW(hResult, L"ورودی نامعتبر است.");
        return;
    }
    wchar_t result[512];
    if (cal == 0)
    {
        persian_date_t p = gregorian_to_persian(year, mon, day);
        wnsprintfW(result, 512, L"%d %s %d میلادی  =  %d %ls %d شمسی",
                   day, gregorian_months[(mon - 1) % 12], year,
                   p.day, months[(p.month - 1) % 12], p.year);
    }
    else
    {
        gregorian_date_t g = persian_to_gregorian(year, mon, day);
        wnsprintfW(result, 512, L"%d %ls %d شمسی  =  %d %ls %d میلادی",
                   day, months[(mon - 1) % 12], year,
                   g.day, gregorian_months[(g.month - 1) % 12], g.year);
    }
    SetWindowTextW(hResult, result);
}

static LRESULT CALLBACK ConvertDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HINSTANCE hInst = reinterpret_cast<HMODULE>(&__ImageBase);
        // Calendar type
        CreateWindowExW(WS_EX_RIGHT, L"STATIC", L"تقویم:",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT,
                        10, 18, 90, 22, hwnd, nullptr, hInst, nullptr);
        HWND hCal = CreateWindowExW(0, L"COMBOBOX", nullptr,
                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                        105, 15, 220, 200, hwnd,
                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_cal_combo_id)), hInst, nullptr);
        SendMessageW(hCal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"میلادی (Gregorian)"));
        SendMessageW(hCal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"شمسی (Persian)"));
        SendMessageW(hCal, CB_SETCURSEL, 0, 0);
        // Day
        CreateWindowExW(WS_EX_RIGHT, L"STATIC", L"روز:",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT,
                        10, 53, 40, 22, hwnd, nullptr, hInst, nullptr);
        HWND hDay = CreateWindowExW(0, L"COMBOBOX", nullptr,
                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                        55, 50, 65, 350, hwnd,
                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_day_combo_id)), hInst, nullptr);
        for (int i = 1; i <= 31; i++)
        {
            wchar_t buf[4];
            wnsprintfW(buf, 4, L"%d", i);
            SendMessageW(hDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
        }
        SendMessageW(hDay, CB_SETCURSEL, 0, 0);
        // Month
        CreateWindowExW(WS_EX_RIGHT, L"STATIC", L"ماه:",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT,
                        130, 53, 40, 22, hwnd, nullptr, hInst, nullptr);
        HWND hMon = CreateWindowExW(0, L"COMBOBOX", nullptr,
                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                        175, 50, 65, 400, hwnd,
                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_mon_combo_id)), hInst, nullptr);
        for (int i = 1; i <= 12; i++)
        {
            wchar_t buf[4];
            wnsprintfW(buf, 4, L"%d", i);
            SendMessageW(hMon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
        }
        SendMessageW(hMon, CB_SETCURSEL, 0, 0);
        // Year
        CreateWindowExW(WS_EX_RIGHT, L"STATIC", L"سال:",
                        WS_CHILD | WS_VISIBLE | SS_RIGHT,
                        250, 53, 40, 22, hwnd, nullptr, hInst, nullptr);
        CreateWindowExW(WS_EX_LEFT, L"EDIT", nullptr,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                        295, 50, 90, 22, hwnd,
                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_year_edit_id)), hInst, nullptr);
        // Convert button
        CreateWindowExW(0, L"BUTTON", L"تبدیل",
                        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                        168, 87, 100, 28, hwnd,
                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_convert_btn_id)), hInst, nullptr);
        // Result label
        CreateWindowExW(WS_EX_RIGHT | WS_EX_RTLREADING, L"STATIC", L"",
                        WS_CHILD | WS_VISIBLE | SS_CENTER,
                        10, 132, 415, 50, hwnd,
                        reinterpret_cast<HMENU>(static_cast<uintptr_t>(dlg_result_lbl_id)), hInst, nullptr);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == dlg_convert_btn_id)
            do_conversion(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void open_convert_dialog()
{
    static const wchar_t *const kClassName = L"PersCalConvertDlg";
    static bool s_registered = false;
    if (!s_registered)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc   = ConvertDlgProc;
        wc.hInstance     = reinterpret_cast<HMODULE>(&__ImageBase);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
        wc.lpszClassName = kClassName;
        wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        s_registered = true;
    }
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kClassName,
        L"تبدیل تاریخ",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 240,
        nullptr, nullptr, reinterpret_cast<HMODULE>(&__ImageBase), nullptr);
    if (hwnd)
        ShowWindow(hwnd, SW_SHOW);
}

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
    typedef BOOL(WINAPI * func_t)(DPI_AWARENESS_CONTEXT);
    func_t func = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext")));
    if (func)
        func(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

static void enable_dark_mode_support()
{
    typedef INT(WINAPI * func_t)(INT); // undocumented SetPreferredAppMode's signature
    func_t func = reinterpret_cast<func_t>(reinterpret_cast<void *>(
        GetProcAddress(GetModuleHandleA("uxtheme.dll"), MAKEINTRESOURCEA(135))));
    if (func)
        func(/*Allow dark*/ 1);
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
        else if (wparam == convert_id)
        {
            open_convert_dialog();
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

extern "C" [[noreturn]] void start();
void start()
{
    HANDLE mutex = CreateMutexA(nullptr, 0, const_cast<char *>(appId));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(EXIT_FAILURE);

    HWND hwnd = CreateWindowExA(
        0, "STATIC", nullptr, 0, 0, 0, 0, 0, nullptr, nullptr,
        reinterpret_cast<HMODULE>(&__ImageBase), nullptr);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));

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

    ExitProcess(msg.wParam);
}
