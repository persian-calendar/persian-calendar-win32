#include "shared.hh"
#include <ShlObj.h>
#include <Shlwapi.h>
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

// Appends s to d and returns the new end, so calls chain without a CRT.
static wchar_t *put(wchar_t *d, const wchar_t *s)
{
    while ((*d = *s++))
        d++;
    return d;
}

static bool known_folder(const KNOWNFOLDERID &id, wchar_t *out)
{
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p)))
        return false;
    put(out, p);
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

static void wait_and_close(SHELLEXECUTEINFOW &sei)
{
    WaitForSingleObject(sei.hProcess, 5000);
    CloseHandle(sei.hProcess);
}

// Asks the tray window to run its "Exit" menu command (id 1011), falling back to taskkill.
static void kill_app()
{
    if (HWND w = FindWindowW(APP_ID, nullptr))
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
        PostMessageW(w, WM_COMMAND, exit_id, 0);
        if (proc)
        {
            WaitForSingleObject(proc, 3000);
            CloseHandle(proc);
        }
    }
    SHELLEXECUTEINFOW sei;
    for (volatile char *b = reinterpret_cast<volatile char *>(&sei); b < reinterpret_cast<volatile char *>(&sei + 1); b++)
        *b = 0;
    sei.cbSize = sizeof sei;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = L"taskkill.exe";
    sei.lpParameters = L"/f /im " APP_EXE;
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei) && sei.hProcess)
        wait_and_close(sei);
}

struct Paths
{
    wchar_t startup[MAX_PATH], programs[MAX_PATH], dir[MAX_PATH];
    wchar_t appExe[MAX_PATH], setupExe[MAX_PATH], lnk[MAX_PATH];
};

static bool get_paths(Paths &p)
{
    wchar_t local[MAX_PATH];
    if (!known_folder(FOLDERID_Startup, p.startup) || !known_folder(FOLDERID_Programs, p.programs) ||
        !known_folder(FOLDERID_LocalAppData, local))
        return false;
    put(put(p.dir, local), L"\\PersianCalendar");
    put(put(put(p.appExe, p.startup), L"\\"), APP_EXE);
    put(put(p.setupExe, p.dir), L"\\setup.exe");
    put(put(put(p.lnk, p.programs), L"\\"), APP_NAME L".lnk");
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
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&sl)))
        return false;
    sl->SetPath(target);
    sl->SetDescription(APP_NAME);
    IPropertyStore *ps = nullptr;
    if (SUCCEEDED(sl->QueryInterface(IID_IPropertyStore, (void **)&ps)))
    {
        set_prop(ps, 5, APP_ID);
        set_prop(ps, 37, uninstCmd);
        ps->Commit();
        ps->Release();
    }
    IPersistFile *pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void **)&pf)))
    {
        ok = SUCCEEDED(pf->Save(lnk, TRUE));
        pf->Release();
    }
    sl->Release();
    return ok;
}

static void set_str(HKEY k, const wchar_t *name, const wchar_t *v)
{
    DWORD n = 0;
    while (v[n])
        n++;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v, (n + 1) * sizeof(wchar_t));
}

static void set_one(HKEY k, const wchar_t *name)
{
    DWORD one = 1;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&one, sizeof one);
}

static UINT install(const Paths &p)
{
    kill_app();
    CreateDirectoryW(p.dir, nullptr);
    wchar_t self[MAX_PATH], cmd[MAX_PATH + 16], silentCmd[MAX_PATH + 32];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    put(put(put(cmd, L"\""), p.setupExe), L"\" /uninstall");
    put(put(put(silentCmd, L"\""), p.setupExe), L"\" /silent-uninstall");
    if (!write_file(p.appExe, payload, sizeof payload) || !CopyFileW(self, p.setupExe, FALSE) ||
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
    // The running setup.exe can't delete itself; let a detached cmd do it after we exit.
    static wchar_t cmd[3 * MAX_PATH];
    wchar_t *e = put(cmd, L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"");
    e = put(put(e, p.setupExe), L"\" & rmdir \"");
    put(put(e, p.dir), L"\"");
    STARTUPINFOW si;
    for (volatile char *b = reinterpret_cast<volatile char *>(&si); b < reinterpret_cast<volatile char *>(&si + 1); b++)
        *b = 0;
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return 0;
}

static bool g_dark;
static bool g_confirmed;
static HBRUSH g_background;

static LRESULT CALLBACK confirm_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO || LOWORD(wParam) == IDCANCEL)
        {
            g_confirmed = LOWORD(wParam) == IDYES;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wParam), g_dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_background);
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
    g_dark = is_dark_mode_active();
    // GDI text doesn't write alpha, so over glass it's invisible; a color key keeps it opaque.
    constexpr COLORREF colorKey = RGB(0xFE, 0x01, 0xFD);
    g_background = CreateSolidBrush(colorKey);

    WNDCLASSW wc;
    zero_memory(wc);
    wc.lpfnWndProc = confirm_window_procedure;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_background;
    wc.lpszClassName = L"InstallerConfirm";
    RegisterClassW(&wc);

    HDC screen = GetDC(nullptr);
    const int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    auto px = [dpi](int v) { return MulDiv(v, dpi, 96); };

    const DWORD style = WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_COMPOSITED;
    RECT rc = {0, 0, px(400), px(130)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr);
    SetLayeredWindowAttributes(hwnd, colorKey, 0, LWA_COLORKEY);

    NONCLIENTMETRICSW ncm;
    zero_memory(ncm);
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    // Antialiased edges would blend with the magenta color key.
    ncm.lfMessageFont.lfQuality = NONANTIALIASED_QUALITY;
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    ncm.lfMessageFont.lfHeight = px(17);
    HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);

    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, px(20), px(20), px(360), px(56), hwnd, nullptr, wc.hInstance, nullptr);
    HWND yesButton = CreateWindowExW(0, L"BUTTON", yes, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, px(120), px(88), px(120), px(28), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDYES)), wc.hInstance, nullptr);
    HWND noButton = CreateWindowExW(0, L"BUTTON", no, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, px(260), px(88), px(120), px(28), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDNO)), wc.hInstance, nullptr);
    HWND controls[] = {label, yesButton, noButton};
    for (HWND h : controls)
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SetFocus(noButton);

    BOOL dark = g_dark;
    if (auto set_attribute = LibraryLoader("dwmapi.dll").getProcedure<HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD)>("DwmSetWindowAttribute"))
        set_attribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof dark);
    if (auto set_theme = LibraryLoader("uxtheme.dll").getProcedure<HRESULT(WINAPI *)(HWND, LPCWSTR, LPCWSTR)>("SetWindowTheme"))
        for (HWND h : controls)
            set_theme(h, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

    glass_window(hwnd, g_dark);

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0)
        if (!IsDialogMessageW(hwnd, &m))
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    return g_confirmed;
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
    UINT code = 1;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    static Paths p;
    if (get_paths(p))
    {
        const wchar_t *args = GetCommandLineW();
        bool installed = GetFileAttributesW(p.appExe) != INVALID_FILE_ATTRIBUTES;
        bool silent = StrStrW(args, L"/silent") != nullptr;

        // Checked first because "/silent-uninstall" also contains "/silent".
        if (StrStrW(args, L"/silent-uninstall"))
            code = uninstall(p);
        else if (StrStrW(args, L"/uninstall") || installed)
        {
            code = 0;
            if (silent && !StrStrW(args, L"/uninstall"))
                code = install(p);
            else if (ask(installed ? L"تقویم فارسی از قبل نصب شده است. آیا می‌‌خواهید آن را حذف کنید؟"
                                   : L"آیا می‌خواهید تقویم فارسی را حذف نصب کنید؟",
                         L"حذف نصب"))
                code = uninstall(p);
        }
        else if (silent || ask(L"آیا می‌خواهید تقویم فارسی را نصب کنید؟", L"نصب"))
            code = install(p);
        else if (confirm(L"تقویم فارسی",
                         installed ? L"تقویم فارسی از قبل نصب شده است. آیا می‌‌خواهید آن را حذف کنید؟"
                                   : L"آیا می‌خواهید تقویم فارسی را حذف نصب کنید؟",
                         L"حذف نصب", L"خیر"))
            code = uninstall(p);
        else
            code = 0;
    }
    ExitProcess(code);
}
