::::::: expand archive :::::::

@echo on
setlocal

set PATH7ZEXE=%ProgramFiles%\7-Zip\7z.exe

if exist "%PATH7ZEXE%" (
    "%PATH7ZEXE%" x -y -o%2 %1
) else (
	powershell -Command "Expand-Archive -Path %1 -DestinationPath %2"
)

