#!/bin/bash
set -euo pipefail

MINGW_PREFIX="$(brew --prefix mingw-w64)"
SYSROOT="$MINGW_PREFIX/toolchain-i686"
MINGW_INCLUDES=("-I$SYSROOT/i686-w64-mingw32/include")

# Prefer Homebrew's LLVM if it's installed; fall back to whatever's on PATH.
if command -v brew &>/dev/null && brew --prefix llvm &>/dev/null; then
    LLVM_PREFIX="$(brew --prefix llvm)"
    CLANG="$LLVM_PREFIX/bin/clang"
    WINDRES="$LLVM_PREFIX/bin/llvm-windres"
    echo "==> Using Homebrew LLVM at $LLVM_PREFIX"
else
    CLANG="${CLANG:-clang}"
    WINDRES="${WINDRES:-llvm-windres}"
    echo "==> Using system clang"
fi

STRIP="$LLVM_PREFIX/bin/llvm-strip"
OBJCOPY="$LLVM_PREFIX/bin/llvm-objcopy"

COMMON=(
    --target=i686-w64-mingw32
    --sysroot="$SYSROOT"
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++
    -std=c++23
    -Wno-c++98-compat-pedantic
    -fno-builtin -fno-threadsafe-statics -ftrivial-auto-var-init=pattern
    -fno-exceptions -fno-rtti -fsafe-buffer-usage-suggestions
    -flto -Oz
    -nostdlib -nodefaultlibs -nostartfiles
    -fuse-ld=lld
    -Wl,-e,start
    -Wl,-subsystem,windows
    -Wl,--disable-reloc-section
    -Wl,--build-id=none
    -ffunction-sections -fdata-sections
    -Wl,--gc-sections
)

LIBS_MAIN=(-lkernel32 -luser32 -lshell32 -lgdi32 -ladvapi32 -lshlwapi)
LIBS_SETUP=("${LIBS_MAIN[@]}" -lole32 -lgdiplus -luuid -mno-stack-arg-probe)

echo "==> Building PersianCalendar.exe"
"$WINDRES" "${MINGW_INCLUDES[@]}" persian-calendar.rc -o persian-calendar.res
"$CLANG" "${COMMON[@]}" persian-calendar.cc persian-calendar.res \
    -o PersianCalendar.exe "${LIBS_MAIN[@]}"
"$STRIP" --strip-all PersianCalendar.exe
python3 postlink.py PersianCalendar.exe

echo "==> Building PersianCalendarSetup.exe"
"$WINDRES" "${MINGW_INCLUDES[@]}" setup.rc -o setup.res
"$CLANG" "${COMMON[@]}" -Wno-c23-extensions setup.cc setup.res \
    -o PersianCalendarSetup.exe "${LIBS_SETUP[@]}"
"$STRIP" --strip-all PersianCalendarSetup.exe
python3 postlink.py PersianCalendarSetup.exe

if command -v wine &>/dev/null; then
    echo "==> Smoke testing under Wine"
    wine PersianCalendar.exe /portable &
    WINE_APP_PID=$!
    wine PersianCalendarSetup.exe &
    WINE_SETUP_PID=$!

    sleep 5

    kill "$WINE_APP_PID"   "$WINE_SETUP_PID" 2>/dev/null || true
    wait "$WINE_APP_PID"   2>/dev/null || true
    wait "$WINE_SETUP_PID" 2>/dev/null || true
fi

echo "==> Done."
