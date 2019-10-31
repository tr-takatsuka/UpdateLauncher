
setlocal
cd /d %~dp0

@echo on

set FNAME_BOOST=boost_1_71_0
set FNAME_ZLIB=zlib-1.2.11

::::::: zlib :::::::

del %FNAME_ZLIB%.tar.gz
rd /s /q %FNAME_ZLIB%
:: powershell -Command "Invoke-WebRequest -Uri https://www.zlib.net/%FNAME_ZLIB%.tar.gz -OutFile %FNAME_ZLIB%.tar.gz"
wsl wget https://www.zlib.net/%FNAME_ZLIB%.tar.gz
wsl tar -xvf %FNAME_ZLIB%.tar.gz


::::::: boost :::::::

del %FNAME_BOOST%.zip
rd /s /q %FNAME_BOOST%
:: powershell -Command "Invoke-WebRequest -Uri https://dl.bintray.com/boostorg/release/1.71.0/source/%FNAME_BOOST%.zip -OutFile %FNAME_BOOST%.zip"
wsl wget https://dl.bintray.com/boostorg/release/1.71.0/source/%FNAME_BOOST%.zip

call expandArchive %FNAME_BOOST%.zip .

cd .\%FNAME_BOOST%

call bootstrap.bat
b2 toolset=msvc-14.1 ^
define=BOOST_USE_WINAPI_VERSION=0x0501 ^
variant=debug,release ^
threading=multi ^
link=static ^
runtime-link=static ^
asmflags=\safeseh ^
address-model=32 ^
-sZLIB_SOURCE="%CD%\..\%FNAME_ZLIB%" ^
-j4



cmd /k
