@echo off
setlocal

REM Visual Studio 2026 installs under "18" instead of the year.
set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
call "%VCVARS%" >nul || exit /b 1

set COMMON=/utf-8 /O1 /GS- /Zl /Gy /EHs-c- /GR- ^
    /std:c++latest /permissive- /Zc:__cplusplus /Zc:preprocessor ^
    /Wall /WX ^
    /wd4710 /wd4711 /wd4820 /wd5045 ^
    /external:anglebrackets /external:W0

set LINKOPT=/link /ENTRY:start /NODEFAULTLIB /SUBSYSTEM:WINDOWS /INCREMENTAL:NO ^
    /OPT:REF /OPT:ICF /FIXED /MERGE:.rdata=.text /ignore:4254

rc /nologo persian-calendar.rc || exit /b 1
cl persian-calendar.cc persian-calendar.res /FePersianCalendar.msvc.exe ^
    %COMMON% %LINKOPT% ^
    kernel32.lib user32.lib shell32.lib gdi32.lib advapi32.lib shlwapi.lib || exit /b 1
python postlink.py PersianCalendar.exe || exit /b 1

REM setup.cc needs #mebed which isn't support in MSVC side
@REM  rc /nologo setup.rc || exit /b 1
@REM  cl setup.cc setup.res /FePersianCalendarSetup.msvc.exe ^
@REM      %COMMON% %LINKOPT% ^
@REM      kernel32.lib user32.lib shell32.lib gdi32.lib advapi32.lib shlwapi.lib ^
@REM      ole32.lib oleaut32.lib gdiplus.lib || exit /b 1
@REM  python postlink.py PersianCalendarSetup.exe || exit /b 1

endlocal
