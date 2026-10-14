@echo off
setlocal

if not defined VSCMD_VER call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1

cl persian-calendar.cc /FePersianCalendar.msvc.exe ^
    /utf-8 /O1 /Wall /GS- /wd4710 /wd4711 /wd4820 /wd5045 /WX /std:c++latest /permissive- ^
    /external:anglebrackets /external:W0 ^
    /link /ENTRY:start /NODEFAULTLIB /SUBSYSTEM:WINDOWS /INCREMENTAL:NO ^
    kernel32.lib user32.lib shell32.lib gdi32.lib advapi32.lib shlwapi.lib ^
    /FIXED /MERGE:.rdata=.text /ignore:4254

endlocal
