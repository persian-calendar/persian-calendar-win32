@echo off
setlocal

REM https://github.com/niXman/mingw-builds-binaries/
set "MINGW=C:\mingw32\bin"

set COMMON=-Wall -Wextra -Wpedantic -Werror -Weffc++ -std=c++2c ^
    -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast ^
    -ffreestanding -fno-threadsafe-statics ^
    -ftrivial-auto-var-init=pattern ^
    -fno-exceptions -Oz -s ^
    -nostdlib -nodefaultlibs -nostartfiles ^
    -Wl,-e,start -Wl,-subsystem,windows

"%MINGW%\windres" persian-calendar.rc persian-calendar_rc.o || exit /b 1
"%MINGW%\i686-w64-mingw32-gcc" persian-calendar.cc persian-calendar_rc.o -o PersianCalendar.gcc.exe ^
    %COMMON% -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 -lshlwapi || exit /b 1
python postlink.py PersianCalendar.exe || exit /b 1

"%MINGW%\windres" setup.rc setup_rc.o || exit /b 1
"%MINGW%\i686-w64-mingw32-gcc" setup.cc setup_rc.o -o PersianCalendarSetup.gcc.exe ^
    %COMMON% -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 -lshlwapi -lole32 -lgdiplus || exit /b 1
python postlink.py PersianCalendarSetup.exe || exit /b 1

endlocal