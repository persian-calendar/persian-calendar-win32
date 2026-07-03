@echo off
REM https://releases.llvm.org/download.html Install from the Windows installer, e.g. LLVM-18.1.8-win32.exe
REM https://github.com/runestubbe/Crinkler - Crinkler.exe must be in the project directory

"C:\Program Files\LLVM\bin\clang" --target=i686-pc-windows-msvc ^
    -c persian-calendar.cc -o persian-calendar.obj ^
    -Weverything -Wall -Wextra -Wpedantic -Werror -Weffc++ ^
    -Wno-c++98-compat-pedantic -Wno-nonportable-system-include-path -Wno-reserved-identifier ^
    -fno-exceptions -fno-rtti -Oz
IF ERRORLEVEL 1 EXIT /B 1

REM Detect Windows 10 SDK x86 lib path from registry
FOR /F "tokens=2*" %%i IN ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 2^>nul') DO SET WinKitsRoot=%%j
IF NOT DEFINED WinKitsRoot FOR /F "tokens=2*" %%i IN ('reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 2^>nul') DO SET WinKitsRoot=%%j
FOR /F "delims=" %%i IN ('dir /B /AD "%WinKitsRoot%Lib" 2^>nul') DO SET WinKitsVer=%%i
SET WinKitsLib=%WinKitsRoot%Lib\%WinKitsVer%\um\x86

.\Crinkler.exe /OUT:persian-calendar.exe ^
    /SUBSYSTEM:WINDOWS /ENTRY:start /NODEFAULTLIB ^
    /COMPMODE:VERYSLOW /HASHTRIES:100 /HASHSIZE:100 /ORDERTRIES:1000 ^
    /TRANSFORM:CALLS /UNALIGNCODE ^
    /LIBPATH:"%WinKitsLib%" ^
    persian-calendar.obj ^
    kernel32.lib user32.lib shell32.lib gdi32.lib shlwapi.lib advapi32.lib
