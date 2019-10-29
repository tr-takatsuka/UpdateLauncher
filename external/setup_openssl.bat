setlocal
cd /d %~dp0

@echo on

del openssl-1.1.1d.tar.gz
rd /s /q openssl-1.1.1d

:: powershell -Command "Invoke-WebRequest -Uri https://www.openssl.org/source/openssl-1.1.1d.tar.gz -OutFile openssl-1.1.1d.tar.gz"
wsl wget https://www.openssl.org/source/openssl-1.1.1d.tar.gz
wsl tar -xvf openssl-1.1.1d.tar.gz

cd ./openssl-1.1.1d

perl Configure VC-WIN32 --prefix=%CD%\x86 --openssldir=%CD%\ssl no-asm no-shared
nmake.exe install

cmd /k
