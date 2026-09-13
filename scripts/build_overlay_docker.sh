#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." >/dev/null 2>&1 && pwd)"
cd "$DIR"

echo "========================================================"
echo "  Compilation de switch-ocr.ovl via Docker"
echo "========================================================"

if ! command -v docker &> /dev/null; then
    echo "[ERREUR] Docker n'est pas installé."
    echo "Vous pouvez compiler le .ovl directement sur GitHub via GitHub Actions."
    exit 1
fi

docker run --rm -v "$DIR":/workspace -w /workspace/switch-overlay devkitpro/devkita64:latest bash -c "
    git clone https://github.com/WerWolv/libtesla.git /tmp/libtesla && \
    cd /tmp/libtesla && make install && \
    cd /workspace/switch-overlay && make
"

if [ -f "switch-overlay/switch-ocr.ovl" ]; then
    echo "[SUCCÈS] switch-ocr.ovl généré dans switch-overlay/switch-ocr.ovl !"
else
    echo "[ERREUR] Échec de la compilation."
    exit 1
fi
