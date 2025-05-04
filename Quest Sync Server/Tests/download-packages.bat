@echo off
echo Downloading NuGet packages for Quest Sync Server Tests...

:: Create packages directory if it doesn't exist
if not exist packages mkdir packages

:: Download NuGet.exe if it doesn't exist
if not exist nuget.exe powershell -Command "Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile nuget.exe"

:: Install Google Test package
nuget.exe install Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn -Version 1.8.1.7 -OutputDirectory packages

echo Done!
pause
