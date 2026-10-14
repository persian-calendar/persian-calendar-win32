@echo off
setlocal
REM https://github.com/llvm/llvm-project/releases Install from the Windows installer, e.g. LLVM-23.1.3-win64.msi
set "LLVM=C:\Program Files\LLVM\bin"

set COMMON=-Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ -std=c++23 ^
    -Wno-c++98-compat-pedantic -ffreestanding -fno-threadsafe-statics ^
    -fno-exceptions -fno-rtti -fsafe-buffer-usage-suggestions -flto -Oz ^
    -nostdlib -nodefaultlibs -nostartfiles -fuse-ld=lld-link -m32 ^
    -Wl,/opt:ref -Wl,/opt:icf -Wl,/entry:start -Wl,/subsystem:windows -Wl,/fixed -Wl,/merge:.rdata=.text

"%LLVM%\llvm-rc" persian-calendar.rc || exit /b 1
"%LLVM%\clang" persian-calendar.cc persian-calendar.res -o PersianCalendar.exe ^
    %COMMON% -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 -lshlwapi || exit /b 1
python postlink.py PersianCalendar.exe || exit /b 1

"%LLVM%\llvm-rc" setup.rc || exit /b 1
"%LLVM%\clang" setup.cc setup.res -o PersianCalendarSetup.exe ^
    %COMMON% -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 -lshlwapi -lole32 -lgdiplus ^
    -Wno-c23-extensions -mno-stack-arg-probe || exit /b 1
python postlink.py PersianCalendarSetup.exe || exit /b 1

endlocal
