# Switch OCR & Overlay Hoshidicts (Japonais) 🎮📖

Solution complète d'**OCR en jeu** et de **dictionnaire instantané** pour Nintendo Switch (Custom Firmware Atmosphère), alimentée par un serveur portable (Android ou PC) et compatible avec le format de dictionnaire **Hoshidicts / Yomitan**.

---

## Fonctionnalités Principales

- 🔍 **OCR Hybride en Jeu** : Détection et reconnaissance de texte japonais (horizontal et vertical) ultra-rapide (<500ms) via **RapidOCR (ONNX Runtime)** ou **Cloud Vision / Gemini Flash**.
- 📚 **Dictionnaires Hoshidicts & Yomitan** : Import direct d'archives Yomitan standard (`.zip`) ou dossiers Hoshidicts (JMdict, Daijisen, accents de hauteur/pitch accent, kanji, etc.).
- 🕹️ **Curseur Virtuel & Snapping Intelligent** :
  - Déplacement fluide au stick analogique gauche.
  - **Aimantage automatique (snap)** sur les boîtes de texte et mots détectés.
  - Support de l'**écran tactile** en mode portable (un tap = sélection immédiate du mot).
- 🎴 **Cartes de Définition Flottantes** : Affichage dynamique au-dessus ou en-dessous du mot (pour ne jamais masquer le texte d'origine), avec kanji, furigana, pitch accent et définitions numérotées.
- 🔄 **Cycle de Définitions & Mots Composés** : Appui sur **A** ou **R** pour faire défiler les différents sens ou les mots imbriqués.
- ⚡ **Minage Anki en 1 Touche** : Appui sur **Y** pour exporter immédiatement le mot, sa lecture, sa définition, la phrase complète et la capture d'écran vers **AnkiConnect** (et sauvegarde de secours dans `vocab_export.csv`).
- 📡 **Découverte Réseau Automatique** : Balise UDP broadcast sur le réseau local. La Switch trouve automatiquement le smartphone Android ou le PC sans devoir ressaisir l'IP si elle change.

---

## Architecture

```
+-------------------------------------------------------------+
|               Nintendo Switch (Atmosphère CFW)               |
|                                                             |
|  [ Jeu en cours ]                                           |
|        ^                                                    |
|        | (Capture écran via capssc / caps:su)               |
|        v                                                    |
|  [ Tesla Overlay (switch-ocr.ovl) ]                         |
|    - Rendu transparent (libtesla / libnx)                   |
|    - Curseur virtuel (stick gauche + snap intelligent)      |
|    - Support tactile (mode portable)                        |
|    - Carte de définition (mot, furigana, pitch, sens)      |
|    - Découverte UDP automatique du serveur                  |
+-------------------------------------------------------------+
                            |
           (Wi-Fi / Hotspot HTTP POST /api/ocr)
           (Réponse JSON : boîtes + tokens + définitions)
                            v
+-------------------------------------------------------------+
|             Serveur OCR & Dictionnaire (Android / PC)       |
|             FastAPI / Uvicorn (Termux ou PC)                |
|                                                             |
|  1. Moteur OCR Hybride :                                    |
|     - RapidOCR (ONNX Runtime, hors-ligne, rapide <500ms)    |
|     - Fallback / Option Cloud (Google Vision / Gemini Flash)|
|                                                             |
|  2. Moteur de Dictionnaire (Hoshidicts) :                   |
|     - Intégration C++ hoshidicts (CLI / bindings)           |
|     - Parseur Python Yomitan/Hoshidicts natif (fallback)    |
|     - Déinflection japonaise & recherche multi-dictionnaires|
|                                                             |
|  3. Export Anki / Vocabulaire :                             |
|     - Synchronisation AnkiConnect (carte Anki automatique)  |
|     - Export CSV local + image de contexte                  |
|                                                             |
|  4. Découverte Réseau :                                     |
|     - Balise UDP broadcast (port 8764)                      |
+-------------------------------------------------------------+
```

---

## Guide d'Installation & Démarrage

### 1. Côté Serveur sur Android (Termux)

1. Installez l'application [Termux](https://github.com/termux/termux-app/releases) (version GitHub ou F-Droid, pas Google Play).
2. Clonez ou transférez ce projet sur votre téléphone.
3. Dans Termux, exécutez le script d'installation :
   ```bash
   chmod +x scripts/setup_termux.sh
   ./scripts/setup_termux.sh
   ```
4. Placez vos dictionnaires Yomitan (fichiers `.zip`, ex: *JMdict.zip*) dans le dossier :
   ```
   data/dictionaries/
   ```
5. Lancez le serveur :
   ```bash
   python -m server.main
   ```

---

### 2. Côté Serveur sur PC (Alternative)

Si vous préférez exécuter le serveur sur votre PC Windows / Linux / macOS :

```bash
# Installation des dépendances
pip install -r server/requirements.txt

# Lancement en une commande
python -m server.main
# (ou double-clic sur scripts/start_server.bat sous Windows)
```

---

### 3. Côté Nintendo Switch

#### Prérequis :
- Une Nintendo Switch sous **Atmosphère** avec **Tesla-Menu** et **nx-ovlloader** installés.

#### Comment obtenir `switch-ocr.ovl` :
Le fichier `.ovl` est un binaire compilé pour le processeur ARM64 de la Switch. Le code source C++ complet se trouve dans [`switch-overlay/`](switch-overlay/). Vous avez 3 méthodes simples pour générer le binaire :

- **Méthode 1 : Automatique via GitHub Actions (Recommandé, 0 installation)** :
  1. Poussez ce projet sur votre compte GitHub (ou faites un fork).
  2. Allez dans l'onglet **Actions** sur GitHub.
  3. Le workflow **Build Switch OCR Overlay** compile automatiquement le projet et vous fournit le fichier `switch-ocr.ovl` prêt au téléchargement dans les artifacts.

- **Méthode 2 : En local avec Docker (1 clic si Docker Desktop est installé)** :
  - Sous Windows : double-cliquez sur `scripts/build_overlay_docker.bat`
  - Sous Linux/macOS : lancez `./scripts/build_overlay_docker.sh`

- **Méthode 3 : Avec devkitPro installé en local** :
  ```bash
  cd switch-overlay
  make
  ```

#### Installation de l'overlay sur la Switch :
1. Une fois `switch-ocr.ovl` obtenu, copiez-le sur votre carte microSD dans :
   ```
   sdmc:/switch/.overlays/switch-ocr.ovl
   ```
2. (Optionnel) Créez le fichier de configuration `sdmc:/config/switch-ocr/config.ini` :
   ```ini
   [network]
   # Laisser auto_discovery=true pour détection automatique sans IP fixe
   auto_discovery=true
   server_ip=192.168.1.50
   server_port=8766
   discovery_port=8764

   [controls]
   cursor_speed=8
   snap_radius=60
   ```


---

## Contrôles en Jeu (Overlay Tesla)

| Action | Contrôle Switch |
| :--- | :--- |
| **Ouvrir l'overlay** | Combinaison Tesla native (`L + Bas + R-Stick` ou personnalisé) |
| **Déplacer le curseur** | **Stick gauche** ou **Croix directionnelle (D-Pad)** |
| **Sélection tactile** | **Toucher directement un mot** sur l'écran (mode portable) |
| **Faire défiler les définitions** | Touche **A** ou **Gâchette R** |
| **Exporter vers Anki (Minage)** | Touche **Y** |
| **Relancer une capture / scan** | Touche **X** |
| **Fermer l'overlay** | Touche **B** (retour immédiat au jeu) |

---

## Intégration AnkiConnect

Lorsque vous appuyez sur **Y** sur la Switch, le mot sélectionné est envoyé au serveur qui :
1. Crée une carte dans votre collection Anki si AnkiConnect est actif (par défaut sur `http://127.0.0.1:8765`), incluant la définition, le kanji, la lecture en furigana, la phrase complète du jeu et la capture d'écran.
2. Ajoute également la ligne dans le fichier CSV local `data/vocab_export.csv` avec la capture d'écran dans `data/screenshots/` (vous ne perdez jamais aucun mot miné même si Anki est fermé).

---

## Configuration Avancée (Variables d'Environnement)

Vous pouvez personnaliser le comportement du serveur via des variables d'environnement :

| Variable | Valeur par défaut | Description |
| :--- | :--- | :--- |
| `SERVER_HOST` | `0.0.0.0` | Adresse d'écoute HTTP |
| `SERVER_PORT` | `8766` | Port du serveur OCR |
| `DISCOVERY_PORT` | `8764` | Port de broadcast UDP pour auto-découverte |
| `OCR_ENGINE` | `auto` | `auto`, `rapidocr`, `cloud` ou `mock` |
| `GEMINI_API_KEY` | *(vide)* | Clé API pour le mode Cloud OCR haute précision |
| `ANKI_CONNECT_URL`| `http://127.0.0.1:8765` | URL d'AnkiConnect |
| `ANKI_DECK_NAME` | `Switch Japanese` | Nom du paquet Anki de destination |
