@echo off
setlocal DisableDelayedExpansion
rem Build the checked-in native solution without a project generator.
cd /d "%~dp0"
set "BUILD_CONFIG=Release"
if not "%~1"=="" set "BUILD_CONFIG=%~1"
if /i "%BUILD_CONFIG%"=="Debug" goto check_action
if /i "%BUILD_CONFIG%"=="Release" goto check_action
goto bad_action

:check_action
if not "%~3"=="" goto bad_action
if "%~2"=="" goto find_tools
if /i "%~2"=="run" goto find_tools
:bad_action
echo Usage: build.bat [Debug^|Release] [run]
goto fail

:find_tools
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Installer was not found.
    goto fail
)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL=%%I"
if not defined VS_INSTALL (
    echo ERROR: Install the Visual Studio Desktop development with C++ workload.
    goto fail
)
set "MSBUILD=%VS_INSTALL%\MSBuild\Current\Bin\MSBuild.exe"
"%MSBUILD%" "%~dp0AlienShooter.sln" /nologo /v:minimal /p:Configuration=%BUILD_CONFIG% /p:Platform=x64
if errorlevel 1 goto fail
if /i "%~2"=="run" start "" "%~dp0bin\%BUILD_CONFIG%\AlienShooter.exe"
echo Build ready: bin\%BUILD_CONFIG%\AlienShooter.exe

@pause
exit /b 0

:fail
rem Keep the window open so a double-clicked build shows its errors.
echo Build failed.
pause
exit /b 1
