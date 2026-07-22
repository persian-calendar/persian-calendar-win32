@echo off
REM call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
cl persian-calendar.cc /Fepersian-calendar.msvc.exe ^
    /utf-8 /O1 /Wall /GS- /wd4710 /wd4711 /wd5045 /WX /std:c++latest ^
    /link /ENTRY:start /NODEFAULTLIB /SUBSYSTEM:WINDOWS,5.01 /INCREMENTAL:NO ^
    kernel32.lib user32.lib shell32.lib gdi32.lib advapi32.lib ^
    /FIXED /MERGE:.rdata=.text /ignore:4254
