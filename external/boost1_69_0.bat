
setlocal
cd /d %~dp0

@echo on

::::::: zlib :::::::

del zlib-1.2.11.tar.gz
:: powershell -Command "Invoke-WebRequest -Uri https://www.zlib.net/zlib-1.2.11.tar.gz -OutFile zlib-1.2.11.tar.gz"
wsl wget https://www.zlib.net/zlib-1.2.11.tar.gz
wsl tar -xvf zlib-1.2.11.tar.gz


::::::: boost :::::::

del boost_1_69_0.zip
:: powershell -Command "Invoke-WebRequest -Uri https://dl.bintray.com/boostorg/release/1.69.0/source/boost_1_69_0.zip -OutFile %BOOSTFILE%"
wsl wget https://dl.bintray.com/boostorg/release/1.69.0/source/boost_1_69_0.zip

call expandArchive boost_1_69_0.zip .

cd .\boost_1_69_0

call bootstrap.bat
b2 toolset=msvc-14.1 ^
define=BOOST_USE_WINAPI_VERSION=0x0501 ^
variant=debug,release ^
threading=multi ^
link=static ^
runtime-link=static ^
asmflags=\safeseh ^
address-model=32 ^
-sZLIB_SOURCE="%CD%\..\zlib-1.2.11" ^
-j4

