@echo off
echo ========================================================
echo   Lancement du serveur Switch OCR & Hoshidicts
echo ========================================================
cd /d "%~dp0\.."
set PYTHON_EXEC=python
if exist "%USERPROFILE%\.venv\Scripts\python.exe" (
    set "PYTHON_EXEC=%USERPROFILE%\.venv\Scripts\python.exe"
) else if exist ".venv\Scripts\python.exe" (
    set "PYTHON_EXEC=.venv\Scripts\python.exe"
)

"%PYTHON_EXEC%" -m server.main
pause
