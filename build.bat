@echo off
REM https://github.com/llvm/llvm-project/releases Install from the Windows installer, e.g. LLVM-22.1.8-win64.exe
"C:\Program Files\LLVM\bin\clang" persian-calendar.cc -o persian-calendar.exe ^
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ ^
    -Wno-c++98-compat-pedantic  -Wno-c++17-attribute-extensions ^
    -Wno-nonportable-system-include-path -Wno-reserved-identifier ^
    -fno-exceptions -fno-rtti -Oz -lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 ^
    -nostdlib -nodefaultlibs -nostartfiles -fuse-ld=lld-link -m32 ^
    -Wl,/entry:start -Wl,/subsystem:windows ^
    -Wl,/fixed -Wl,/merge:.rdata=.text
