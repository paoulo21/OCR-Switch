@echo off
echo ========================================================
echo  Compilation de switch-ocr.ovl via Docker (devkitPro)
echo ========================================================

where docker >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERREUR] Docker n'est pas installe ou pas dans le PATH.
    echo Vous pouvez soit :
    echo   1. Installer Docker Desktop pour compiler localement.
    echo   2. Pousser ce projet sur GitHub (le workflow compile automatiquement le .ovl).
    echo   3. Installer devkitPro sous Windows avec le composant switch-dev.
    pause
    exit /b 1
)

cd /d "%~dp0\.."
docker run --rm -v "%cd%":/workspace -w /workspace/switch-overlay devkitpro/devkita64:latest bash -c "git clone https://github.com/WerWolv/libtesla.git /tmp/libtesla && cd /tmp/libtesla && make install && cd /workspace/switch-overlay && make"

if exist "switch-overlay\switch-ocr.ovl" (
    echo.
    echo [SUCCES] switch-ocr.ovl a ete compile avec succes dans switch-overlay\switch-ocr.ovl !
) else (
    echo.
    echo [ERREUR] La compilation a echoue. Verifiez les logs ci-dessus.
)
pause
