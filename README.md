A simple and small (5kb) way to display Persian calendar as a tray icon in your Windows machine.

برنامهٔ سادهٔ و کوچک (۵ کیلوبایت) نمایش تقویم فارسی به‌عنوان یک آیکون در نوار اعلان ویندوز.

It looks something like this

<img width="372" height="231" alt="image" src="https://github.com/user-attachments/assets/e15b9ce4-16f1-4a78-9309-8e431eb523a9" />

and it has a support from Windows XP to Windows 11's HiDPI and dark mode for the tray menu. 

Installation
------------

* Find a `persian-calendar.exe` from the latest version of https://github.com/persian-calendar/persian-calendar-win32/releases
* Press Win+E (brings the Explorer) then Ctrl+L (opens the location bar), then type `shell:startup` and enter
* Drop the `persian-calendar.exe` on the folder

Build
-----

Windows with MSVC
* Find a 32bit llvm installation file from, e.g. LLVM-18.1.8-win32.exe,
from https://releases.llvm.org/download.html
* Make sure Visual Studio is installed from llvm (llvm uses Visual Studio provided headers)
* Run `build.bat`
* Execute `postlink.py` to get the same binary (which patches the exe to support older Windows versions and removes its build timestamp)

MingW in Linux/macOS
* `./build.sh`
