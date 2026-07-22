A simple and small (9kb) way to display Persian calendar as a tray icon in your Windows machine and a date converter.

برنامهٔ سادهٔ و کوچک (۹ کیلوبایت) نمایش تقویم فارسی به‌عنوان یک آیکون در نوار اعلان ویندوز و یک مبدل تاریخ.

It looks something like this

<img width="383" height="274" alt="image" src="https://github.com/user-attachments/assets/a9fcef64-7496-442f-a97c-b7f2c3c4a4a3" />
<img width="645" height="277" alt="image" src="https://github.com/user-attachments/assets/8a8a690a-e9cf-4f00-8a42-f474136c51b0" />
<img width="881" height="319" alt="image" src="https://github.com/user-attachments/assets/3efd2d00-9fe0-4892-966d-9c4b9a7a7d87" />

and it has a support from Windows XP to Windows 11's HiDPI and dark mode for the tray menu. 

Installation
------------

* Find a `persian-calendar.exe` from the latest version of https://github.com/persian-calendar/persian-calendar-win32/releases
* Press Win+R and type `shell:startup`
* Drop the `persian-calendar.exe` on the folder

Build
-----

Windows with MSVC
* https://github.com/llvm/llvm-project/releases Install from the Windows installer, e.g. LLVM-22.1.8-win64.exe
* Make sure Visual Studio is installed (llvm uses Visual Studio provided headers)
* Run `build.bat`
* Execute `postlink.py` to get the same binary (which patches the exe to support older Windows versions and removes its build timestamp)

MingW in Linux/macOS
* `./build.sh`
