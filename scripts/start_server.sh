#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." >/dev/null 2>&1 && pwd)"
cd "$DIR"

IP=$(ip route get 1.1.1.1 2>/dev/null | awk '{print $7}')
if [ -z "$IP" ]; then
    IP=$(ifconfig 2>/dev/null | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1' | head -n1)
fi

echo "========================================================"
echo "  Serveur Switch OCR & Hoshidicts"
if [ -n "$IP" ]; then
    echo "  Adresse IP de votre telephone : $IP"
    echo "  Port HTTP                     : 8766"
fi
echo "========================================================"
python -m server.main
