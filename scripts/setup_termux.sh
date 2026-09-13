#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
# Script d'installation et de configuration automatique pour Termux (Android)
# Projet : Switch OCR & Hoshidicts
# ==============================================================================

set -e

echo "=== [1/4] Mise à jour des paquets Termux ==="
pkg update -y
pkg install -y python git clang libjpeg-turbo freetype
pkg install -y python-pillow || true

echo "=== [2/4] Installation des dépendances Python ==="
# Sous Android/Termux, Pydantic v1 (pur Python) évite la compilation lourde de pydantic-core en Rust
pip install "pydantic<2" "fastapi<=0.125.0" uvicorn requests python-multipart
pip install pillow || true

# Tentative d'installation de RapidOCR si architecture compatible
echo "=== [3/4] Installation optionnelle de RapidOCR (ONNX) ==="
pip install onnxruntime rapidocr-onnxruntime 2>/dev/null || echo "Note: RapidOCR sera utilisé en mode cloud ou fallback si onnxruntime n'est pas disponible pour cette version Android."

echo "=== [4/4] Création des dossiers de données ==="
mkdir -p data/dictionaries
mkdir -p data/screenshots

echo ""
echo "=================================================================="
echo " Installation terminée avec succès !"
echo " Pour ajouter vos dictionnaires Yomitan (.zip) :"
echo "   Copiez-les dans le dossier : data/dictionaries/"
echo ""
echo " Pour lancer le serveur :"
echo "   python -m server.main"
echo "=================================================================="
