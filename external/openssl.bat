setlocal
cd /d %~dp0

@echo on

del openssl-1.1.1a.tar.gz
:: powershell -Command "Invoke-WebRequest -Uri https://www.openssl.org/source/openssl-1.1.1a.tar.gz -OutFile openssl-1.1.1a.tar.gz"
wsl wget https://www.openssl.org/source/openssl-1.1.1a.tar.gz
wsl tar -xvf openssl-1.1.1a.tar.gz

cd ./openssl-1.1.1a

perl Configure VC-WIN32 --prefix=%CD%\x86 --openssldir=%CD%\ssl no-asm no-shared
nmake.exe install

cd ..

