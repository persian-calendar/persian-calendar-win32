@echo off
REM https://github.com/niXman/mingw-builds-binaries/
C:\mingw32\bin\i686-w64-mingw32-gcc persian-calendar.cc -o persian-calendar.gcc.exe ^
    -Wall -Wextra -Wpedantic -Werror -Weffc++ -fno-exceptions -Oz -s ^
    -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 ^
    -nostdlib -nodefaultlibs -nostartfiles -Wl,-e,start -Wl,-subsystem,windows
