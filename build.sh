clang --target=i686-w64-mingw32 --sysroot=$(brew --prefix mingw-w64)/toolchain-i686 \
    persian-calendar.cc -o persian-calendar.exe \
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ \
    -Wno-c++98-compat-pedantic -Wno-nonportable-system-include-path -Wno-reserved-identifier \
    -Wno-old-style-cast -Wno-sign-conversion -Wno-sign-compare \
    -fno-exceptions -fno-rtti -Oz -lkernel32 -luser32 -lshell32 -lgdi32 -lshlwapi -ladvapi32 \
    -nostdlib -nodefaultlibs -nostartfiles -fuse-ld=lld \
    -Wl,-e,start -Wl,-subsystem,windows \
    -Wl,--disable-reloc-section -Wl,--build-id=none \
    && ./postlink.py && wine persian-calendar.exe
