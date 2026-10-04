@echo off
taskkill /IM PersianCalendar.exe /FI "STATUS eq RUNNING" ^
    && "C:\Program Files\LLVM\bin\clang" test.cc -D_CRT_SECURE_NO_WARNINGS -o test.exe && test.exe ^
    && build.bat && python postlink.py ^
    && python -c "d=open('PersianCalendar.exe', 'rb').read(); print(f'PersianCalendar.exe: {len(d)}-{len(d) - len(d.rstrip(b'\xcc'))}')" ^
    && python -c "d=open('PersianCalendarInstaller.exe', 'rb').read(); print(f'PersianCalendarInstaller.exe: {len(d)}-{len(d) - len(d.rstrip(b'\xcc'))}')" ^
    && start /b PersianCalendar.exe ^
    && pause && taskkill /IM PersianCalendar.exe /FI "STATUS eq RUNNING"
REM dumpbin /DISASM persian-calendar.exe
REM     && "C:\Program Files\LLVM\bin\clang-tidy" -checks="-*,bugprone-*,modernize-*,-modernize-avoid-c-arrays" persian-calendar.cc -- -std=c++23 ^
