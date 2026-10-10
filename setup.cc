#include "shared.hh"
#include <ShlObj.h>
#include <objbase.h>
#include <propsys.h>
#include <propkey.h>

#include "setup-res.h"

static const unsigned char appExe[] = {
#embed "PersianCalendar.exe"
};

#define APP_NAME L"Persian Calendar"
#define APP_EXE  L"PersianCalendar.exe"
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PersianCalendarWin32"
#define STARTUP_KEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

static auto known_folder(const KNOWNFOLDERID &id, wchar_t *out) -> bool
{
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p)))
        return false;
    lstrcpyW(out, p);
    CoTaskMemFree(p);
    return true;
}

static auto write_file(const wchar_t *path, const void *data, DWORD size) -> bool
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
        WaitForSingleObject(sei.hProcess, 2500);
        CloseHandle(sei.hProcess);
    }
}

struct Paths
{
    wchar_t dir[MAX_PATH], appExe[MAX_PATH], uninstallExe[MAX_PATH], self[MAX_PATH], lnk[MAX_PATH];
};

static auto get_paths(Paths &p) -> bool
{
    wchar_t userprograms[MAX_PATH];
    wchar_t programs[MAX_PATH];
    if (!known_folder(FOLDERID_Programs, programs) || !known_folder(FOLDERID_UserProgramFiles, userprograms))
        return false;
    GetModuleFileNameW(nullptr, p.self, MAX_PATH);
    wsprintfW(p.dir, L"%s\\" APP_ID, userprograms);
    wsprintfW(p.appExe, L"%s\\" APP_EXE, p.dir);
    wsprintfW(p.uninstallExe, L"%s\\PersianCalendarSetup.exe", p.dir);
    wsprintfW(p.lnk, L"%s\\" APP_NAME L".lnk", programs);
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

static auto make_shortcut(const wchar_t *lnk, const wchar_t *iconPath, const wchar_t *target, const wchar_t *uninstCmd) -> bool
{
    IShellLinkW *sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&sl))))
        return false;
    sl->SetIconLocation(iconPath, 0);
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

static auto install(const Paths &p) -> UINT
{
    kill_app();
    CreateDirectoryW(p.dir, nullptr);
    wchar_t cmd[MAX_PATH + 16], silentCmd[MAX_PATH + 32];
    wsprintfW(cmd, L"\"%s\"", p.uninstallExe);
    wsprintfW(silentCmd, L"\"%s\" /silent", p.uninstallExe);
    if (!write_file(p.appExe, appExe, sizeof appExe) ||
        !CopyFileW(p.self, p.uninstallExe, FALSE) ||
        !make_shortcut(p.lnk, p.uninstallExe, p.appExe, silentCmd))
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
        set_str(k, L"DisplayIcon", p.uninstallExe);
        set_str(k, L"InstallLocation", p.dir);
        set_one(k, L"NoModify");
        set_one(k, L"NoRepair");
        RegCloseKey(k);
    }
    // Add to startup
    if (RegCreateKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(k, APP_ID, 0, REG_SZ, reinterpret_cast<const BYTE *>(p.appExe), static_cast<DWORD>((lstrlenW(p.appExe) + 1) * static_cast<int>(sizeof(wchar_t))));
        RegCloseKey(k);
    }
    return 0;
}

static auto uninstall(const Paths &p) -> UINT
{
    kill_app();
    DeleteFileW(p.appExe);
    DeleteFileW(p.lnk);
    RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_KEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\" APP_ID);
    {
        HKEY k;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS)
        {
            RegDeleteValueW(k, APP_NAME);
            RegCloseKey(k);
        }
    }
    // The running PersianCalendarSetup.exe can't delete itself; let a helper cmd do it after we exit.
    wchar_t cmd[3 * MAX_PATH];
    wsprintfW(cmd, L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"%s\" & rmdir \"%s\"", p.uninstallExe, p.dir);
    STARTUPINFOW si;
    zero_memory(si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;

    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) == 0)
        lstrcpyW(tempPath, L"C:\\");

    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, tempPath, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 5000);
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
    HWND icon;
    HICON hIcon;
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

    void reloadIcon(int dpi)
    {
        if (!icon)
            return;
        const int size = MulDiv(32, dpi, 96);
        HICON fresh = static_cast<HICON>(
            LoadImageW(GetModuleHandleW(nullptr),
                       MAKEINTRESOURCEW(IDI_APP_ICON),
                       IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
        if (!fresh)
            return;
        if (hIcon)
            DestroyIcon(hIcon);
        hIcon = fresh;
        SendMessageW(icon, STM_SETICON, reinterpret_cast<WPARAM>(hIcon), 0);
        MoveWindow(icon, MulDiv(20, dpi, 96), MulDiv(24, dpi, 96), size, size, TRUE);
    }

    void updateLayout(int dpi)
    {
        auto px = [dpi](int v)
        { return MulDiv(v, dpi, 96); };

        reloadIcon(dpi);
        MoveWindow(label, px(64), px(20), px(316), px(56), TRUE);
        MoveWindow(yesButton, px(195), px(88), px(90), px(28), TRUE);
        MoveWindow(noButton, px(295), px(88), px(90), px(28), TRUE);
        NONCLIENTMETRICSW ncm;
        zero_memory(ncm);
        ncm.cbSize = sizeof ncm;
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
        ncm.lfMessageFont.lfWeight = FW_NORMAL;
        {
            ncm.lfMessageFont.lfHeight = px(16);
            HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);
            SendMessageW(yesButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(noButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        {
            // Antialiased edges would blend with the magenta color key.
            ncm.lfMessageFont.lfQuality = NONANTIALIASED_QUALITY;
            ncm.lfMessageFont.lfHeight = px(18);
            HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);
            SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
    }
};

#define WM_DPICHANGED 0x02E0

static auto CALLBACK confirm_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
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
    case WM_DPICHANGED:
        state->updateLayout(HIWORD(wParam));
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

static auto confirm(const wchar_t *title, const wchar_t *text, const wchar_t *yes, const wchar_t *no) -> bool
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

    const int dpi = static_cast<int>(get_system_dpi());
    auto px = [dpi](int v)
    { return MulDiv(v, dpi, 96); };

    const DWORD style = WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_RTLREADING | WS_EX_LAYOUTRTL | WS_EX_LAYERED | WS_EX_COMPOSITED;
    RECT rc = {0, 0, px(400), px(130)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
    SetLayeredWindowAttributes(hwnd, colorKey, 0, LWA_COLORKEY);

    state.label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, wc.hInstance, nullptr);
    state.icon  = CreateWindowExW(0, L"STATIC", nullptr,
                                  WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
                                  0, 0, 0, 0, hwnd, nullptr, wc.hInstance, nullptr);
    state.yesButton = CreateWindowExW(WS_EX_COMPOSITED, L"BUTTON", yes, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDYES)), wc.hInstance, nullptr);
    state.noButton = CreateWindowExW(WS_EX_COMPOSITED, L"BUTTON", no, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDNO)), wc.hInstance, nullptr);
    state.updateLayout(dpi);
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

    if (state.hIcon)
        DestroyIcon(state.hIcon);

    return state.confirmed;
}

static auto ask(const wchar_t *text, const wchar_t *yes) -> bool
{
    return confirm(L"تقویم فارسی", text, yes, L"خیر");
}

extern "C" [[noreturn]] void start();
void start()
{
    enable_hidpi();
    enable_dark_mode_support();
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        ExitProcess(1);
    UINT code = 0;
    Paths p;
    if (get_paths(p))
    {
        bool isInstalled = GetFileAttributesW(p.uninstallExe) != INVALID_FILE_ATTRIBUTES;
        bool isSelfUninstaller = CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE,
                                                p.self, -1, p.uninstallExe, -1) == CSTR_EQUAL;

        // Get rid of this some day
        if (!isInstalled && !isSelfUninstaller)
        {
            wchar_t oldDir[2 * MAX_PATH], oldUninstall[2 * MAX_PATH];
            wsprintfW(oldDir, L"%s\\" APP_ID, p.dir);
            wsprintfW(oldUninstall, L"%s\\setup.exe", oldDir);
            if (GetFileAttributesW(oldUninstall) != INVALID_FILE_ATTRIBUTES &&
                CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE,
                               p.self, -1, oldUninstall, -1) != CSTR_EQUAL)
            {
                ShellExecuteW(nullptr, L"open", oldUninstall, nullptr, oldDir, SW_SHOWNORMAL);
                CoUninitialize();
                ExitProcess(0);
            }
        }

        if (isInstalled && !isSelfUninstaller)
            ShellExecuteW(nullptr, L"open", p.uninstallExe, nullptr, p.dir, SW_SHOWNORMAL);
        else
        {
            bool isSilent = StrStrW(GetCommandLineW(), L"/silent") != nullptr;
            if (isInstalled)
            {
                if (isSilent || ask(L"مایلید تقویم فارسی را حذف نصب کنید؟", L"حذف نصب"))
                    code = uninstall(p);
            }
            else if (isSilent || ask(L"مایلید تقویم فارسی را نصب کنید؟", L"نصب"))
            {
                code = install(p);
                if (code == 0 && !isSilent)
                    ShellExecuteW(nullptr, L"open", p.appExe, nullptr, p.dir, SW_SHOWNORMAL);
            }
        }
    }
    CoUninitialize();
    ExitProcess(code);
}
