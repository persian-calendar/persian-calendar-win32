A simple and small (9kb) way to display Persian calendar as a tray icon in your Windows machine.

برنامهٔ سادهٔ و کوچک (۹ کیلوبایت) نمایش تقویم فارسی به‌عنوان یک آیکون در نوار اعلان ویندوز.

It looks something like this

<img width="360" height="321" alt="image" src="https://github.com/user-attachments/assets/b5a576e4-698d-45ec-913e-504c989c56c0" />
<img width="528" height="130" alt="image" src="https://github.com/user-attachments/assets/b7ba8877-14ba-46ce-b703-3d92037f9e29" />

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
