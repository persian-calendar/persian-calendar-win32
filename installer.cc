#include "shared.hh"
#include <ShlObj.h>
#include <objbase.h>
#include <propsys.h>
#include <propkey.h>

static const unsigned char payload[] = {
#embed "PersianCalendar.exe"
};

#define APP_NAME L"Persian Calendar"
#define APP_EXE L"PersianCalendar.exe"
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PersianCalendarWin32"
#define TILE_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Start\\TileProperties\\W~" APP_ID

static bool known_folder(const KNOWNFOLDERID &id, wchar_t *out)
{
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p)))
        return false;
    lstrcpyW(out, p);
    CoTaskMemFree(p);
    return true;
}

static bool write_file(const wchar_t *path, const void *data, DWORD size)
{
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    DWORD w = 0;
    bool ok = WriteFile(f, data, size, &w, nullptr) && w == size;
    CloseHandle(f);
    return ok;
}

static void kill_app()
{
    SHELLEXECUTEINFOW sei;
    zero_memory(sei);
    sei.cbSize = sizeof sei;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = L"taskkill.exe";
    sei.lpParameters = L"/f /im " APP_EXE;
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei) && sei.hProcess)
    {
        WaitForSingleObject(sei.hProcess, 5000);
        CloseHandle(sei.hProcess);
    }
}

struct Paths
{
    wchar_t startup[MAX_PATH], programs[MAX_PATH], dir[MAX_PATH];
    wchar_t appExe[MAX_PATH], uninstallerExe[MAX_PATH], lnk[MAX_PATH];
};

static bool get_paths(Paths &p)
{
    wchar_t local[MAX_PATH];
    if (!known_folder(FOLDERID_Startup, p.startup) || !known_folder(FOLDERID_Programs, p.programs) ||
        !known_folder(FOLDERID_LocalAppData, local))
        return false;
    wsprintfW(p.dir, L"%s\\" APP_ID, local);
    wsprintfW(p.appExe, L"%s\\" APP_EXE, p.startup);
    wsprintfW(p.uninstallerExe, L"%s\\unins000.exe", p.dir);
    wsprintfW(p.lnk, L"%s\\" APP_NAME L".lnk", p.programs);
    return true;
}

// System.AppUserModel.ID (5) and .UninstallCommand (37) make Start show an inline Uninstall that runs the command directly.
static void set_prop(IPropertyStore *ps, DWORD pid, const wchar_t *v)
{
    const PROPERTYKEY key = {{0x9F4C2855, 0x9F79, 0x4B39, {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}}, pid};
    PROPVARIANT pv;
    pv.vt = VT_LPWSTR;
    pv.pwszVal = const_cast<LPWSTR>(v);
    ps->SetValue(key, pv);
}

static bool make_shortcut(const wchar_t *lnk, const wchar_t *target, const wchar_t *uninstCmd)
{
    IShellLinkW *sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&sl))))
        return false;
    sl->SetPath(target);
    sl->SetDescription(APP_NAME);
    IPropertyStore *ps = nullptr;
    if (SUCCEEDED(sl->QueryInterface(IID_IPropertyStore, reinterpret_cast<void **>(&ps))))
    {
        set_prop(ps, 5, APP_ID);
        set_prop(ps, 37, uninstCmd);
        ps->Commit();
        ps->Release();
    }
    IPersistFile *pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&pf))))
    {
        ok = SUCCEEDED(pf->Save(lnk, TRUE));
        pf->Release();
    }
    sl->Release();
    return ok;
}

static void set_str(HKEY k, const wchar_t *name, const wchar_t *v)
{
    DWORD n = static_cast<DWORD>(lstrlenW(v));
    RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(v), (n + 1) * sizeof(wchar_t));
}

static void set_one(HKEY k, const wchar_t *name)
{
    DWORD one = 1;
    RegSetValueExW(k, name, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&one), sizeof one);
}

static UINT install(const Paths &p)
{
    kill_app();
    CreateDirectoryW(p.dir, nullptr);
    wchar_t self[MAX_PATH], cmd[MAX_PATH + 16], silentCmd[MAX_PATH + 32];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    wsprintfW(cmd, L"\"%s\"", p.uninstallerExe);
    wsprintfW(silentCmd, L"\"%s\" /silent", p.uninstallerExe);
    if (!write_file(p.appExe, payload, sizeof payload) || !CopyFileW(self, p.uninstallerExe, FALSE) ||
        !make_shortcut(p.lnk, p.appExe, silentCmd))
    {
        MessageBoxW(nullptr, L"Installation failed.", APP_NAME, MB_ICONERROR);
        return 1;
    }
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS)
    {
        set_str(k, L"DisplayName", APP_NAME);
        set_str(k, L"UninstallString", cmd);
        set_str(k, L"QuietUninstallString", silentCmd);
        set_str(k, L"DisplayIcon", p.appExe);
        set_str(k, L"InstallLocation", p.dir);
        set_one(k, L"NoModify");
        set_one(k, L"NoRepair");
        RegCloseKey(k);
    }
    // Category 2 is Productivity in the Start menu's "All apps" grouping.
    if (RegCreateKeyExW(HKEY_CURRENT_USER, TILE_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS)
    {
        DWORD productivity = 2;
        RegSetValueExW(k, L"Category", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&productivity), sizeof productivity);
        RegCloseKey(k);
    }
    ShellExecuteW(nullptr, L"open", p.appExe, nullptr, p.startup, SW_SHOWNORMAL);
    return 0;
}

static UINT uninstall(const Paths &p)
{
    kill_app();
    DeleteFileW(p.appExe);
    DeleteFileW(p.lnk);
    RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_KEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, TILE_KEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\" APP_ID);
    // The running unins000.exe can't delete itself; let a detached cmd do it after we exit.
    wchar_t cmd[3 * MAX_PATH];
    wsprintfW(cmd, L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"%s\" & rmdir \"%s\"", p.uninstallerExe, p.dir);
    STARTUPINFOW si;
    zero_memory(si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return 0;
}

struct dialog_state_t
{
    HBRUSH background;
    BOOL dark;
    BOOL confirmed;
    HWND label;
    HWND yesButton;
    HWND noButton;

    void darkModeUpdate(HWND hwnd)
    {
        dark = is_dark_mode_active();
        set_immersive_dark_mode(hwnd, dark);
        if (auto set_theme = LibraryLoader("uxtheme.dll").getProcedure<HRESULT(WINAPI *)(HWND, LPCWSTR, LPCWSTR)>("SetWindowTheme"))
        {
            set_theme(label, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            set_theme(yesButton, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            set_theme(noButton, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
    }
};

static LRESULT CALLBACK confirm_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<dialog_state_t *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_SETTINGCHANGE:
        state->darkModeUpdate(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO || LOWORD(wParam) == IDCANCEL)
        {
            state->confirmed = LOWORD(wParam) == IDYES;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wParam), state->dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(state->background);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool confirm(const wchar_t *title, const wchar_t *text, const wchar_t *yes, const wchar_t *no)
{
    dialog_state_t state;
    zero_memory(state);
    state.dark = is_dark_mode_active();
    state.background = CreateSolidBrush(colorKey);

    WNDCLASSW wc;
    zero_memory(wc);
    wc.lpfnWndProc = confirm_window_procedure;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = state.background;
    wc.lpszClassName = L"InstallerConfirm";
    RegisterClassW(&wc);

    HDC screen = GetDC(nullptr);
    const int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    auto px = [dpi](int v)
    { return MulDiv(v, dpi, 96); };

    const DWORD style = WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_COMPOSITED;
    RECT rc = {0, 0, px(400), px(130)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
    SetLayeredWindowAttributes(hwnd, colorKey, 0, LWA_COLORKEY);

    state.label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, px(20), px(20), px(360), px(56), hwnd, nullptr, wc.hInstance, nullptr);
    state.yesButton = CreateWindowExW(0, L"BUTTON", yes, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, px(120), px(88), px(120), px(28), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDYES)), wc.hInstance, nullptr);
    state.noButton = CreateWindowExW(0, L"BUTTON", no, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, px(260), px(88), px(120), px(28), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDNO)), wc.hInstance, nullptr);
    {
        NONCLIENTMETRICSW ncm;
        zero_memory(ncm);
        ncm.cbSize = sizeof ncm;
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
        ncm.lfMessageFont.lfWeight = FW_NORMAL;
        {
            ncm.lfMessageFont.lfHeight = px(16);
            HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);
            SendMessageW(state.yesButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(state.noButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        {
            // Antialiased edges would blend with the magenta color key.
            ncm.lfMessageFont.lfQuality = NONANTIALIASED_QUALITY;
            ncm.lfMessageFont.lfHeight = px(18);
            HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);
            SendMessageW(state.label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
    }
    glass_window(hwnd);
    state.darkModeUpdate(hwnd);
    SetFocus(state.noButton);

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0)
        if (!IsDialogMessageW(hwnd, &m))
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    return state.confirmed;
}

static bool ask(const wchar_t *text, const wchar_t *yes)
{
    return confirm(L"تقویم فارسی", text, yes, L"خیر");
}

extern "C" [[noreturn]] void start();
void start()
{
    enable_hidpi();
    enable_dark_mode_support();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Paths p;
    if (!get_paths(p))
        ExitProcess(1);

    bool isSilent = StrStrW(GetCommandLineW(), L"/silent") != nullptr;
    UINT code = 0;
    if (GetFileAttributesW(p.appExe) != INVALID_FILE_ATTRIBUTES)
    {
        if (isSilent || ask(L"آیا می‌خواهید تقویم فارسی را حذف نصب کنید؟", L"حذف نصب"))
            code = uninstall(p);
    }
    else if (isSilent || ask(L"آیا می‌خواهید تقویم فارسی را نصب کنید؟", L"نصب"))
        code = install(p);

    ExitProcess(code);
}
