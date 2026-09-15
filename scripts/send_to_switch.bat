@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   Envoi automatique de switch-ocr.ovl vers la Switch
echo ========================================================
echo.

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "OVL_FILE="

rem 1. Trouver le fichier .ovl
if exist "%ROOT_DIR%\switch-ocr.ovl" (
    set "OVL_FILE=%ROOT_DIR%\switch-ocr.ovl"
) else if exist "%ROOT_DIR%\switch-overlay\switch-ocr.ovl" (
    set "OVL_FILE=%ROOT_DIR%\switch-overlay\switch-ocr.ovl"
) else if exist "%USERPROFILE%\Downloads\switch-ocr.ovl" (
    set "OVL_FILE=%USERPROFILE%\Downloads\switch-ocr.ovl"
)

if "%OVL_FILE%"=="" (
    echo [ERREUR] Impossible de trouver le fichier switch-ocr.ovl !
    echo Placez le fichier switch-ocr.ovl dans le dossier du projet ou dans vos Telechargements.
    echo.
    pause
    exit /b 1
)

echo Fichier trouve : %OVL_FILE%
echo.

rem 2. Recuperer l'IP de la Switch
set "IP_FILE=%ROOT_DIR%\data\switch_ip.txt"
set "DEFAULT_IP=192.168.1."
if exist "%IP_FILE%" (
    set /p DEFAULT_IP=<"%IP_FILE%"
)

echo Entrez l'adresse IP de votre Switch (affiche dans FTPD ou DBI) :
set /p SWITCH_IP="[Defaut: %DEFAULT_IP%]: "
if "%SWITCH_IP%"=="" (
    set "SWITCH_IP=%DEFAULT_IP%"
)

rem Sauvegarder pour la prochaine fois
if not exist "%ROOT_DIR%\data" mkdir "%ROOT_DIR%\data"
echo %SWITCH_IP%> "%IP_FILE%"

echo.
echo Envoi en cours vers ftp://%SWITCH_IP%:5000/switch/.overlays/ ...
curl.exe -T "%OVL_FILE%" "ftp://%SWITCH_IP%:5000/switch/.overlays/switch-ocr.ovl" --connect-timeout 5

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================================
    echo   [SUCCES] switch-ocr.ovl a ete copie sur votre Switch !
    echo   Ouvrez simplement le menu Tesla sur votre console.
    echo   Aucun redemarrage n'est necessaire !
    echo ========================================================
) else (
    echo.
    echo [ECHEC] Impossible de joindre le serveur FTP sur la Switch.
    echo Verifications :
    echo   1. Lancez l'application FTPD sur votre Switch (ou activez sys-ftpd).
    echo   2. Verifiez que la Switch et le PC sont sur le meme Wi-Fi.
    echo   3. Verifiez l'adresse IP de la Switch (%SWITCH_IP%).
)

echo.
pause
