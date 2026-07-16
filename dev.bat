@echo off
taskkill /IM persian-calendar.exe /FI "STATUS eq RUNNING" ^
    && "C:\Program Files\LLVM\bin\clang" test.cc -D_CRT_SECURE_NO_WARNINGS -o test.exe && test.exe ^
    && build.bat && python postlink.py && dir persian-calendar*.exe && start /b persian-calendar.exe ^
    && pause && taskkill /IM persian-calendar.exe /FI "STATUS eq RUNNING"
REM dumpbin /DISASM persian-calendar.exe
