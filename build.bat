@echo off
REM https://github.com/llvm/llvm-project/releases Install from the Windows installer, e.g. LLVM-22.1.8-win64.exe
"C:\Program Files\LLVM\bin\clang" persian-calendar.cc -o PersianCalendar.exe ^
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ -std=c++23 ^
    -Wno-c++98-compat-pedantic ^
    -fno-exceptions -fno-rtti -fsafe-buffer-usage-suggestions -flto ^
    -Oz -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 ^
    -nostdlib -nodefaultlibs -nostartfiles -fuse-ld=lld-link -m32 ^
    -Wl,/entry:start -Wl,/subsystem:windows -Wl,/fixed -Wl,/merge:.rdata=.text

"C:\Program Files\LLVM\bin\clang" installer.cc -o PersianCalendarInstaller.exe ^
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ -std=c++23 -Wno-c23-extensions ^
    -mno-stack-arg-probe ^
    -Wno-c++98-compat-pedantic ^
    -Wno-unsafe-buffer-usage -Wno-old-style-cast ^
    -fno-exceptions -fno-rtti -fno-builtin ^
    -Oz -flto -m32 -fuse-ld=lld-link -nostdlib -nodefaultlibs -nostartfiles ^
    -lkernel32 -luser32 -lshell32 -lole32 -ladvapi32 -lshlwapi -lgdi32 ^
    -Wl,/entry:start -Wl,/subsystem:windows -Wl,/fixed -Wl,/merge:.rdata=.text ^
    -Wl,/manifest:embed "-Wl,/manifestuac:level='asInvoker' uiAccess='false'" ^
    "-Wl,/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
