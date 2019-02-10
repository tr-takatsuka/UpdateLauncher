setlocal
cd /d %~dp0

@echo on

del yaml-cpp-yaml-cpp-0.6.2.zip
:: powershell -Command "Invoke-WebRequest -Uri https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.2.zip -OutFile yaml-cpp-0.6.2.zip"
wsl wget https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.2.zip

call expandArchive yaml-cpp-0.6.2.zip .

set BuildDir=.\yaml-cpp-yaml-cpp-0.6.2build

cmake -S .\yaml-cpp-yaml-cpp-0.6.2 -B "%BuildDir%" -G "Visual Studio 14 2015" ^
-D YAML_CPP_BUILD_TESTS=OFF ^
-D YAML_CPP_BUILD_TOOLS=OFF ^
-D MSVC_SHARED_RT=OFF

call .\msbuild.bat "%BuildDir%\INSTALL.vcxproj" /p:Platform="Win32";Configuration=Debug
call .\msbuild.bat "%BuildDir%\INSTALL.vcxproj" /p:Platform="Win32";Configuration=Release

