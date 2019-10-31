setlocal
cd /d %~dp0

@echo on

set VERSION=0.6.3

:: clear ::::::::::::::
del yaml-cpp-%VERSION%.zip
rd /s /q yaml-cpp-yaml-cpp-%VERSION%
rd /s /q yaml-cpp-yaml-cpp-%VERSION%build

:: download ::::::::::::::
:: powershell -Command "Invoke-WebRequest -Uri https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-%VERSION%.zip -OutFile yaml-cpp-%VERSION%.zip"
wsl wget https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-%VERSION%.zip


call expandArchive yaml-cpp-%VERSION%.zip .

set BuildDir=.\yaml-cpp-yaml-cpp-%VERSION%build

cmake -S .\yaml-cpp-yaml-cpp-%VERSION% -B "%BuildDir%" -G "Visual Studio 15 2017" ^
-D YAML_CPP_BUILD_TESTS=OFF ^
-D YAML_CPP_BUILD_TOOLS=OFF ^
-D YAML_MSVC_SHARED_RT=OFF

call .\msbuild.bat "%BuildDir%\INSTALL.vcxproj" /p:Platform="Win32";Configuration=Debug
call .\msbuild.bat "%BuildDir%\INSTALL.vcxproj" /p:Platform="Win32";Configuration=Release

cmd /k
