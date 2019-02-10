:: MSBuild.exe %*

@echo off
setlocal
rem cd /d %~dp0

set PF32=%ProgramFiles(x86)%
if not exist "%PF32%" set PF32=%ProgramFiles%

set VsWherePath="%PF32%\Microsoft Visual Studio\Installer\vswhere.exe"

for /f "usebackq tokens=1* delims=: " %%i in (`%VsWherePath% -latest -requires Microsoft.Component.MSBuild`) do (
	if /i "%%i"=="installationPath" set MSBuild15Path="%%j\MSBuild\15.0\Bin\MSBuild.exe"
)

if not exist %MSBuild15Path% (
	echo "MSBuild.exe not find"
	exit /b 1
)

echo on
%MSBuild15Path% %*
