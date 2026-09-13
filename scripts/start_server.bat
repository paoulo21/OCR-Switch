@echo off
echo ========================================================
echo   Lancement du serveur Switch OCR & Hoshidicts
echo ========================================================
cd /d "%~dp0\.."
python -m server.main
pause
