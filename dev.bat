@echo off
taskkill /IM persian-calendar.exe /FI "STATUS eq RUNNING" ^
    && "C:\Program Files (x86)\LLVM\bin\clang" test.cc -D_CRT_SECURE_NO_WARNINGS && test.exe ^
    && build.bat && python postlink.py && dir persian-calendar*.exe && persian-calendar.exe
REM dumpbin /DISASM persian-calendar.exe
