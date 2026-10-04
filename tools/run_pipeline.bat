@echo off
rem @brief Preprocess one game package into the output folder beside this script.
setlocal
set "PYTHONDONTWRITEBYTECODE=1"
if "%~1"=="" goto usage
if not "%~2"=="" goto usage
if exist "%~dp0output\.venv\Scripts\python.exe" (
    "%~dp0output\.venv\Scripts\python.exe" -B "%~dp0asset_pipeline\pipeline.py" "%~f1"
    exit /b
)
where py >nul 2>nul
if not errorlevel 1 (
    py -3 -B "%~dp0asset_pipeline\pipeline.py" "%~f1"
    exit /b
)
python -B "%~dp0asset_pipeline\pipeline.py" "%~f1"
exit /b
:usage
echo Usage: run_pipeline.bat "game directory, JAR or ZIP"
exit /b 1
