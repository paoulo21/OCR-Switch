@echo off
echo ========================================================
echo  Compilation de switch-ocr.ovl via Docker (devkitPro)
echo ========================================================

where docker >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERREUR] Docker n'est pas installe ou pas dans le PATH.
    echo Vous pouvez simplement pousser vos modifications sur GitHub :
    echo le workflow GitHub Actions compile automatiquement switch-ocr.ovl.
    pause
    exit /b 1
)

cd /d "%~dp0\.."
docker run --rm -v "%cd%":/workspace -w /workspace/switch-overlay devkitpro/devkita64:latest make -j

if exist "switch-overlay\switch-ocr.ovl" (
    echo.
    echo [SUCCES] switch-ocr.ovl a ete compile avec succes dans switch-overlay\switch-ocr.ovl !
) else (
    echo.
    echo [ERREUR] La compilation a echoue. Verifiez les logs ci-dessus.
)
pause
