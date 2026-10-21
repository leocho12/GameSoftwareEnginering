@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0server.py" --open
    goto finished
)
where python >nul 2>nul
if not errorlevel 1 (
    python "%~dp0server.py" --open
    goto finished
)
if exist "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" (
    "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" "%~dp0server.py" --open
    goto finished
)
echo Python 3 is required.
:finished
pause
endlocal
