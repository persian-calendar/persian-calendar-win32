#!/usr/bin/env python3
import pefile
from pathlib import Path

for exe_file in ['persian-calendar.exe', 'installer.exe']:
    with open(exe_file, 'rb') as f:
        pe_data = f.read()
    pe = pefile.PE(data=pe_data)
    pe.FILE_HEADER.TimeDateStamp = 0
    opt = pe.OPTIONAL_HEADER
    # 4.0 is Windows NT4, the actual support is from Windows XP though 2000 is also tested to work
    opt.MajorOperatingSystemVersion = 4
    opt.MinorOperatingSystemVersion = 0
    opt.MajorSubsystemVersion = 4
    opt.MinorSubsystemVersion = 0
    pe.write(exe_file)
