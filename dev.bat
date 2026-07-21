@echo off
taskkill /IM persian-calendar.exe /FI "STATUS eq RUNNING" ^
    && "C:\Program Files\LLVM\bin\clang" test.cc -D_CRT_SECURE_NO_WARNINGS -o test.exe && test.exe ^
    && build.bat && python postlink.py ^
    && python -c "d=open('persian-calendar.exe', 'rb').read(); print(f'{len(d)}{len(d.rstrip(b'\xcc')) - len(d)}')" ^
    && start /b persian-calendar.exe ^
    && pause && taskkill /IM persian-calendar.exe /FI "STATUS eq RUNNING"
REM dumpbin /DISASM persian-calendar.exe
