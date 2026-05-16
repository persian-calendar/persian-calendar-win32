@echo off
REM https://releases.llvm.org/download.html Install from the Windows installer, e.g. LLVM-18.1.8-win32.exe
"C:\Program Files\LLVM\bin\clang" persian-calendar.cc -o persian-calendar.exe ^
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ ^
    -Wno-c++98-compat-pedantic -Wno-nonportable-system-include-path -Wno-reserved-identifier ^
    -fno-exceptions -fno-rtti -Oz -lkernel32 -luser32 -lshell32 -lgdi32 -lshlwapi -ladvapi32 ^
    -nostdlib -nodefaultlibs -nostartfiles -fuse-ld=lld-link -m32 ^
    -Wl,/entry:start -Wl,/subsystem:windows -Wl,-m32 ^
    -Wl,/fixed -Wl,/merge:.rdata=.text
