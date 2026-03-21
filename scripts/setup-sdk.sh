#!/bin/bash
# Download and install NVIDIA Maxine AFX (and optionally VFX) SDK
#
# Requires: NGC API key (free account at https://catalog.ngc.nvidia.com/)
#
# Usage:
#   ./setup-sdk.sh [OPTIONS] [NGC_API_KEY]
#
# Options:
#   --gpu ARCH        GPU architecture override (default: auto-detect)
#                     Values: l4 (sm_89), a100 (sm_80), t4 (sm_75), v100 (sm_70)
#   --vfx             Also download the Video Effects SDK
#   --effects LIST    Comma-separated effects to download (default: aec,denoiser)
#                     Available: aec, denoiser, dereverb, superres, studio_voice,
#                                speaker_focus, voice_font
#   --sdk-dir DIR     Installation directory (default: /opt/maxine-afx)
#   --uninstall       Remove the SDK installation
#   --help            Show this help

set -euo pipefail

# ---- Defaults ----
SDK_DIR="/opt/maxine-afx"
GPU_ARCH=""
EFFECTS="aec,denoiser"
INSTALL_VFX=false
UNINSTALL=false
NGC_KEY=""
MIN_DISK_MB=5000   # 5 GB minimum free space

# ---- Color output ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${GREEN}==>${NC} ${BOLD}$*${NC}"; }
warn()  { echo -e "${YELLOW}==> WARNING:${NC} $*"; }
error() { echo -e "${RED}==> ERROR:${NC} $*" >&2; }
die()   { error "$@"; exit 1; }

# ---- Parse arguments ----
while [[ $# -gt 0 ]]; do
    case "$1" in
        --gpu)
            GPU_ARCH="$2"; shift 2 ;;
        --effects)
            EFFECTS="$2"; shift 2 ;;
        --sdk-dir)
            SDK_DIR="$2"; shift 2 ;;
        --vfx)
            INSTALL_VFX=true; shift ;;
        --uninstall)
            UNINSTALL=true; shift ;;
        --help|-h)
            head -17 "$0" | tail -15
            exit 0 ;;
        -*)
            die "Unknown option: $1 (use --help)" ;;
        *)
            NGC_KEY="$1"; shift ;;
    esac
done

# ---- Uninstall mode ----
if $UNINSTALL; then
    info "Uninstalling Maxine SDK from $SDK_DIR"

    if [ -f /etc/ld.so.conf.d/maxine-afx.conf ]; then
        info "Removing /etc/ld.so.conf.d/maxine-afx.conf"
        sudo rm -f /etc/ld.so.conf.d/maxine-afx.conf
        sudo ldconfig
    fi

    if [ -d "$SDK_DIR" ]; then
        info "Removing $SDK_DIR"
        sudo rm -rf "$SDK_DIR"
    fi

    info "Uninstall complete."
    exit 0
fi

# ---- Banner ----
echo ""
echo -e "${BOLD}=== NVIDIA Maxine SDK Setup ===${NC}"
echo ""

# ---- Check NGC API key ----
if [ -n "$NGC_KEY" ]; then
    export NGC_API_KEY="$NGC_KEY"
elif [ -z "${NGC_API_KEY:-}" ]; then
    die "NGC API key required.

  1. Create a free account at: https://catalog.ngc.nvidia.com/
  2. Generate an API key at:   https://org.ngc.nvidia.com/setup/api-key
  3. Run:  ./setup-sdk.sh YOUR_API_KEY
     or:   export NGC_API_KEY=YOUR_API_KEY && ./setup-sdk.sh"
fi

# ---- Check nvidia-smi ----
if ! command -v nvidia-smi &>/dev/null; then
    die "nvidia-smi not found. Install NVIDIA drivers first.
  Arch/CachyOS: sudo pacman -S nvidia-utils"
fi

# ---- Detect GPU ----
info "Detecting GPU..."
GPU_INFO=$(nvidia-smi --query-gpu=name,compute_cap,driver_version,memory.total --format=csv,noheader 2>/dev/null) \
    || die "nvidia-smi failed. Is the NVIDIA driver loaded?"

echo "  $GPU_INFO"
echo ""

COMPUTE_CAP=$(echo "$GPU_INFO" | head -1 | cut -d',' -f2 | tr -d ' ')
COMPUTE_MAJOR=$(echo "$COMPUTE_CAP" | cut -d'.' -f1)
COMPUTE_MINOR=$(echo "$COMPUTE_CAP" | cut -d'.' -f2)

# Maxine requires Turing (7.5) or newer for AFX
if [ "$COMPUTE_MAJOR" -lt 7 ] || { [ "$COMPUTE_MAJOR" -eq 7 ] && [ "$COMPUTE_MINOR" -lt 5 ]; }; then
    die "GPU compute capability $COMPUTE_CAP is too low.
  Maxine AFX requires >= 7.5 (RTX 2060 / Turing or newer).
  Your GPU does not have Tensor Cores."
fi

info "GPU compute capability: $COMPUTE_CAP (OK)"

# ---- Auto-detect GPU architecture ----
if [ -z "$GPU_ARCH" ]; then
    case "$COMPUTE_MAJOR.$COMPUTE_MINOR" in
        7.5)  GPU_ARCH="t4" ;;   # Turing
        8.0)  GPU_ARCH="a100" ;;  # Ampere (GA100)
        8.6)  GPU_ARCH="l4" ;;    # Ampere (GA10x)
        8.9)  GPU_ARCH="l4" ;;    # Ada Lovelace
        9.*)  GPU_ARCH="l4" ;;    # Blackwell / future
        *)    GPU_ARCH="l4" ;;    # fallback to latest
    esac
    info "Auto-detected GPU architecture: $GPU_ARCH (sm_${COMPUTE_CAP/./})"
else
    info "Using GPU architecture override: $GPU_ARCH"
fi

# ---- Check available disk space ----
PARENT_DIR=$(dirname "$SDK_DIR")
if [ -d "$PARENT_DIR" ]; then
    AVAIL_MB=$(df -m "$PARENT_DIR" | tail -1 | awk '{print $4}')
    if [ "$AVAIL_MB" -lt "$MIN_DISK_MB" ]; then
        die "Insufficient disk space: ${AVAIL_MB} MB available, need at least ${MIN_DISK_MB} MB.
  Free up space or use --sdk-dir to choose a different location."
    fi
    info "Disk space: ${AVAIL_MB} MB available (need ${MIN_DISK_MB} MB)"
fi

# ---- Create install directory ----
info "Installing to $SDK_DIR ..."
sudo mkdir -p "$SDK_DIR"
sudo chown "$USER:$USER" "$SDK_DIR"

# ---- Download AFX SDK ----
if command -v ngc &>/dev/null; then
    info "Downloading AFX SDK via NGC CLI..."
    ngc registry resource download-version \
        "nvidia/maxine/maxine_linux_audio_effects_sdk:latest" \
        --dest "$SDK_DIR"
else
    warn "NGC CLI not found. Trying direct download..."
    echo ""
    echo "  For best results, install the NGC CLI:"
    echo "    pip install ngc-cli"
    echo ""
    echo "  Manual download available at:"
    echo "    https://catalog.ngc.nvidia.com/orgs/nvidia/teams/maxine/collections/maxine_linux_audio_effects_sdk_collection"
    echo ""
    echo "  Extract to $SDK_DIR so the structure looks like:"
    echo "    $SDK_DIR/nvafx/lib/libnv_audiofx.so"
    echo "    $SDK_DIR/features/aec/..."
    echo "    $SDK_DIR/features/denoiser/..."
    echo "    $SDK_DIR/external/cuda/lib/..."
    echo ""
    die "Cannot proceed without NGC CLI. Install it with: pip install ngc-cli"
fi

# ---- Download VFX SDK (optional) ----
if $INSTALL_VFX; then
    info "Downloading VFX SDK via NGC CLI..."
    ngc registry resource download-version \
        "nvidia/maxine/maxine_linux_video_effects_sdk:latest" \
        --dest "$SDK_DIR"
fi

# ---- Download models for selected effects ----
info "Downloading models for GPU arch $GPU_ARCH, effects: $EFFECTS"
cd "$SDK_DIR"
if [ -x "features/download_features.sh" ]; then
    cd features
    ./download_features.sh --gpu "$GPU_ARCH" --effects "$EFFECTS"
    cd ..
elif [ -d "features" ]; then
    warn "download_features.sh not found, models may need manual download."
    echo "  Check $SDK_DIR/features/ for download instructions."
else
    warn "features/ directory not found. SDK download may have failed."
fi

# ---- Verify installation ----
info "Verifying installation..."
MISSING=0

if [ ! -f "$SDK_DIR/nvafx/lib/libnv_audiofx.so" ]; then
    warn "libnv_audiofx.so not found at $SDK_DIR/nvafx/lib/"
    MISSING=1
fi

IFS=',' read -ra EFFECT_LIST <<< "$EFFECTS"
for effect in "${EFFECT_LIST[@]}"; do
    effect=$(echo "$effect" | tr -d ' ')
    if [ ! -d "$SDK_DIR/features/$effect" ]; then
        warn "Feature directory not found: $SDK_DIR/features/$effect"
        MISSING=1
    fi
done

if [ "$MISSING" -eq 1 ]; then
    warn "Some files are missing. The SDK may not work correctly."
    echo "  Check the NGC download output above for errors."
fi

# ---- Create ld.so config for runtime libraries ----
info "Setting up library paths..."
sudo tee /etc/ld.so.conf.d/maxine-afx.conf > /dev/null <<EOF
$SDK_DIR/nvafx/lib
$SDK_DIR/external/cuda/lib
EOF

# Add feature library paths if they exist
for dir in "$SDK_DIR"/features/*/lib; do
    [ -d "$dir" ] && echo "$dir" | sudo tee -a /etc/ld.so.conf.d/maxine-afx.conf > /dev/null
done

sudo ldconfig

# ---- Summary ----
echo ""
info "Setup complete!"
echo ""
echo "  SDK installed to:  $SDK_DIR"
echo "  GPU architecture:  $GPU_ARCH (compute $COMPUTE_CAP)"
echo "  Effects:           $EFFECTS"
if $INSTALL_VFX; then
    echo "  VFX SDK:           installed"
fi
echo ""
echo "  Next steps:"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "    1. Build:    cd $SCRIPT_DIR/.. && meson setup build && meson compile -C build"
echo "    2. Install:  sudo meson install -C build"
echo "    3. Config:   cp config/60-maxine-echo-cancel.conf ~/.config/pipewire/pipewire.conf.d/"
echo "    4. Restart:  systemctl --user restart pipewire"
echo ""
echo "  Or use the daemon:"
echo "    systemctl --user enable --now maxined"
echo "    maxctl status"
