#include "shared.hh"
#include <ShlObj.h>
#include <objbase.h>
#include <propsys.h>
#include <propkey.h>
#include <gdiplus.h>

#include "setup-rc.h"

static const unsigned char appExe[] = {
#embed "PersianCalendar.exe"
};

#define APP_NAME L"Persian Calendar"
#define APP_EXE L"PersianCalendar.exe"

constexpr auto uninstall_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PersianCalendarWin32";
constexpr auto startup_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto app_title = L"تقویم فارسی";

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

struct Paths
{
    wchar_t dir[MAX_PATH], appExe[MAX_PATH], uninstallExe[MAX_PATH], self[MAX_PATH], lnk[MAX_PATH];
    BOOL failed;

    Paths() : dir{}, appExe{}, uninstallExe{}, self{}, lnk{}, failed(false) {
        using SHGetFolderPathW_t = HRESULT(WINAPI *)(HWND, int, HANDLE, DWORD, LPWSTR);
        // SHGetFolderPathW historically was in shfolder then moved to shell32 in XP
        auto get_folder_path = LibraryLoader("shfolder.dll").getProcedure<SHGetFolderPathW_t>("SHGetFolderPathW");
        if (!get_folder_path)
            get_folder_path = LibraryLoader("shell32.dll").getProcedure<SHGetFolderPathW_t>("SHGetFolderPathW");
        if (!get_folder_path) {
            this->failed = true;
            return;
        }

        wchar_t local[MAX_PATH], programs[MAX_PATH], userprograms[MAX_PATH];
        if (FAILED(get_folder_path(nullptr, CSIDL_PROGRAMS, nullptr, 0, programs)) ||
            FAILED(get_folder_path(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, local))) {
            this->failed = true;
            return;
        }

        wsprintfW(userprograms, L"%s\\Programs", local); // Equivalent to FOLDERID_UserProgramFiles
        CreateDirectoryW(userprograms, nullptr);

        GetModuleFileNameW(nullptr, this->self, MAX_PATH);
        wsprintfW(this->dir, L"%s\\" APP_ID, userprograms);
        wsprintfW(this->appExe, L"%s\\" APP_EXE, this->dir);
        wsprintfW(this->uninstallExe, L"%s\\PersianCalendarSetup.exe", this->dir);
        wsprintfW(this->lnk, L"%s\\" APP_NAME L".lnk", programs);
    }
};

static auto make_shortcut(const wchar_t *lnk, const wchar_t *target, const wchar_t *uninstCmd) -> bool
{
    IShellLinkW *sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&sl))))
        return false;
    sl->SetIconLocation(target, 0);
    sl->SetPath(target);
    sl->SetDescription(APP_NAME);
    IPropertyStore *ps = nullptr;
    if (SUCCEEDED(sl->QueryInterface(IID_IPropertyStore, reinterpret_cast<void **>(&ps))))
    {
        auto make_propvar = [](const wchar_t *v)
        {
            PROPVARIANT pv{};
            zero_memory(pv);
            pv.vt = VT_LPWSTR;
            pv.pwszVal = const_cast<LPWSTR>(v);
            return pv;
        };
        ps->SetValue(PKEY_AppUserModel_ID, make_propvar(APP_ID));
        ps->SetValue(PKEY_AppUserModel_UninstallCommand, make_propvar(uninstCmd));
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

static void run_hidden(const wchar_t *cmd, bool wait)
{
    STARTUPINFOW si;
    zero_memory(si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    zero_memory(pi);
    if (CreateProcessW(nullptr, const_cast<LPWSTR>(cmd), nullptr, nullptr,
                       FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        if (wait)
            WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void kill_process(const wchar_t *exe_name)
{
    wchar_t sysdir[MAX_PATH];
    if (!GetSystemDirectoryW(sysdir, MAX_PATH))
        return;

    wchar_t cmd[MAX_PATH + 64];
    wsprintfW(cmd, L"\"%s\\taskkill.exe\" /f /im %s", sysdir, exe_name);
    run_hidden(cmd, /*wait=*/true);
    Sleep(150);
}

static auto install(const Paths &p) -> UINT
{
    kill_process(APP_EXE);
    CreateDirectoryW(p.dir, nullptr);
    wchar_t cmd[MAX_PATH + 16], silentCmd[MAX_PATH + 32];
    wsprintfW(cmd, L"\"%s\"", p.uninstallExe);
    wsprintfW(silentCmd, L"\"%s\" /silent", p.uninstallExe);
    if (!write_file(p.appExe, appExe, sizeof appExe) ||
        !CopyFileW(p.self, p.uninstallExe, FALSE))
    {
        MessageBoxW(nullptr, L"Failed to copy the files.", app_title, MB_ICONERROR);
        return 1;
    }
    if (!make_shortcut(p.lnk, p.appExe, silentCmd))
    {
        MessageBoxW(nullptr, L"Failed to create shortcut.", app_title, MB_ICONERROR);
        return 1;
    }
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, uninstall_key, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS)
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
    if (RegCreateKeyExW(HKEY_CURRENT_USER, startup_key, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(k, APP_ID, 0, REG_SZ, reinterpret_cast<const BYTE *>(p.appExe), static_cast<DWORD>((lstrlenW(p.appExe) + 1) * static_cast<int>(sizeof(wchar_t))));
        RegCloseKey(k);
    }
    return 0;
}

static auto uninstall(const Paths &p) -> UINT
{
    kill_process(APP_EXE);

    for (int i = 0, done = 0; i < 5 && !done; ++i)
    {
        bool a = DeleteFileW(p.appExe) || GetLastError() == ERROR_FILE_NOT_FOUND;
        bool b = DeleteFileW(p.lnk) || GetLastError() == ERROR_FILE_NOT_FOUND;
        done = a && b;
        if (!done)
            Sleep(150);
    }

    SHDeleteKeyW(HKEY_CURRENT_USER, uninstall_key);
    SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\" APP_ID);
    if (HKEY k; RegOpenKeyExW(HKEY_CURRENT_USER, startup_key, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS)
    {
        RegDeleteValueW(k, APP_ID);
        RegCloseKey(k);
    }

    wchar_t cmd[4 * MAX_PATH];
    wsprintfW(cmd,
              L"cmd.exe /c ping -n 3 127.0.0.1 >nul & "
              L"del /f /q \"%s\" & "
              L"rmdir /q \"%s\"",
              p.uninstallExe, p.dir);
    run_hidden(cmd, /*wait=*/false);
    return 0;
}

static auto load_png_icon(int cx, int cy) -> HICON
{
    HRSRC group = FindResourceW(nullptr, MAKEINTRESOURCEW(IDI_APP_ICON), RT_GROUP_ICON);
    if (!group)
        return nullptr;
    HGLOBAL gh = LoadResource(nullptr, group);
    if (!gh)
        return nullptr;
    IB_WARNING_DISABLE_CLANG_PUSH("-Wunsafe-buffer-usage")
    BYTE *dir = static_cast<BYTE *>(LockResource(gh));
    if (!dir)
        return nullptr;

    WORD count = *reinterpret_cast<WORD *>(dir + 4);
    if (count == 0)
        return nullptr;
    WORD iconId = *reinterpret_cast<WORD *>(dir + 6 + 12);
    IB_WARNING_DISABLE_CLANG_POP

    HRSRC icon = FindResourceW(nullptr, MAKEINTRESOURCEW(iconId), RT_ICON);
    if (!icon)
        return nullptr;
    DWORD size = SizeofResource(nullptr, icon);
    HGLOBAL ih = LoadResource(nullptr, icon);
    if (!ih)
        return nullptr;
    void *bytes = LockResource(ih);
    if (!bytes)
        return nullptr;

    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
        return nullptr;
    ULONG written = 0;
    stream->Write(bytes, size, &written);
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    Gdiplus::GpBitmap *bitmap = nullptr;
    HICON big = nullptr;
    if (Gdiplus::DllExports::GdipCreateBitmapFromStream(stream, &bitmap) == 0 && bitmap)
    {
        Gdiplus::DllExports::GdipCreateHICONFromBitmap(bitmap, &big);
        Gdiplus::DllExports::GdipDisposeImage(bitmap);
    }
    stream->Release();

    if (!big)
        return nullptr;

    HICON scaled = static_cast<HICON>(CopyImage(big, IMAGE_ICON, cx, cy, 0));
    DestroyIcon(big);
    return scaled;
}

struct dialog_state_t
{
    HBRUSH background;
    HBRUSH buttonBackground;
    BOOL dark;
    BOOL confirmed;
    HWND label;
    HWND icon;
    HICON hIcon;
    HWND yesButton;
    HWND noButton;
    ULONG_PTR gdiplustoken = 0;

    dialog_state_t()
        : background(nullptr),
          buttonBackground(nullptr),
          dark(FALSE),
          confirmed(FALSE),
          label(nullptr),
          icon(nullptr),
          hIcon(nullptr),
          yesButton(nullptr),
          noButton(nullptr)
    {
        Gdiplus::GdiplusStartupInput input{};
        if (Gdiplus::GdiplusStartup(&gdiplustoken, &input, nullptr) != 0)
            gdiplustoken = 0;
    }

    ~dialog_state_t()
    {
        if (gdiplustoken)
            Gdiplus::GdiplusShutdown(gdiplustoken);
    }

    void darkModeUpdate(HWND hwnd)
    {
        dark = is_dark_mode_active();
        set_immersive_dark_mode(hwnd, dark);
        if (buttonBackground)
            DeleteObject(buttonBackground);
        buttonBackground = CreateSolidBrush(dark ? RGB(0x2B, 0x2B, 0x2B)
                                                 : RGB(0xF3, 0xF3, 0xF3));
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
        const int size = MulDiv(64, dpi, 96);
        HICON fresh = load_png_icon(size, size);
        if (!fresh)
            return;
        if (hIcon)
            DestroyIcon(hIcon);
        hIcon = fresh;
        SendMessageW(icon, STM_SETICON, reinterpret_cast<WPARAM>(hIcon), 0);
        MoveWindow(icon, MulDiv(16, dpi, 96), MulDiv(16, dpi, 96), size, size, TRUE);
    }

    void updateLayout(int dpi)
    {
        auto px = [dpi](int v)
        { return MulDiv(v, dpi, 96); };

        reloadIcon(dpi);
        MoveWindow(label, px(96), px(20), px(284), px(56), TRUE);
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
    case WM_CREATE:
        toggle_exstyle(hwnd, has_composition(), WS_EX_LAYERED | WS_EX_COMPOSITED);
        break;
    case WM_SETTINGCHANGE:
        toggle_exstyle(hwnd, has_composition(), WS_EX_LAYERED | WS_EX_COMPOSITED);
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
    case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(wParam), state->dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(state->buttonBackground);
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
    dialog_state_t state{};
    state.dark = is_dark_mode_active();
    state.background = CreateSolidBrush(has_composition() ? colorKey : GetSysColor(COLOR_BTNFACE));
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
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_RTLREADING | WS_EX_LAYOUTRTL;
    RECT rc = {0, 0, px(400), px(130)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
    set_layered_window_attributes(hwnd, colorKey, 0, LWA_COLORKEY);

    state.label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, wc.hInstance, nullptr);
    state.icon = CreateWindowExW(0, L"STATIC", nullptr,
                                 WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
                                 0, 0, 0, 0, hwnd, nullptr, wc.hInstance, nullptr);
    state.yesButton = CreateWindowExW(0, L"BUTTON", yes, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDYES)), wc.hInstance, nullptr);
    state.noButton = CreateWindowExW(0, L"BUTTON", no, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDNO)), wc.hInstance, nullptr);
    state.updateLayout(dpi);
    glass_window(hwnd);
    state.darkModeUpdate(hwnd);
    SetFocus(state.yesButton);

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
    if (state.buttonBackground)
        DeleteObject(state.buttonBackground);

    return state.confirmed;
}

static auto ask(const wchar_t *text, const wchar_t *yes) -> bool
{
    return confirm(app_title, text, yes, L"خیر");
}

extern "C" [[noreturn]] void start();
void start()
{
    enable_hidpi();
    enable_dark_mode_support();
    Paths p{};
    if (p.failed)
    {
        MessageBoxW(nullptr, L"Failed to retrieve paths", app_title, MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        ExitProcess(1);

    UINT code = 0;
    bool isInstalled = GetFileAttributesW(p.uninstallExe) != INVALID_FILE_ATTRIBUTES;
    bool isSelfUninstaller = CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE,
                                            p.self, -1, p.uninstallExe, -1) == CSTR_EQUAL;

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

    CoUninitialize();
    ExitProcess(code);
}
