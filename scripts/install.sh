#!/bin/bash
# Build and install Maxine PipeWire
#
# Usage: ./scripts/install.sh [--user] [--prefix /usr/local]
#
# Options:
#   --user          Only enable systemd service (no sudo for service)
#   --prefix DIR    Installation prefix (default: /usr/local)
#   --no-service    Skip systemd service setup
#   --clean         Remove build directory before building

set -euo pipefail

# ---- Defaults ----
PREFIX="/usr/local"
ENABLE_SERVICE=true
USER_ONLY=false
CLEAN=false

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
        --user)
            USER_ONLY=true; shift ;;
        --prefix)
            PREFIX="$2"; shift 2 ;;
        --no-service)
            ENABLE_SERVICE=false; shift ;;
        --clean)
            CLEAN=true; shift ;;
        --help|-h)
            head -10 "$0" | tail -8
            exit 0 ;;
        *)
            die "Unknown option: $1 (use --help)" ;;
    esac
done

# ---- Find project root ----
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

cd "$PROJECT_DIR"

echo ""
echo -e "${BOLD}=== Maxine PipeWire Installation ===${NC}"
echo ""

# ---- Check dependencies ----
info "Checking build dependencies..."

MISSING=()
command -v meson   &>/dev/null || MISSING+=("meson")
command -v ninja   &>/dev/null || MISSING+=("ninja")
command -v pkg-config &>/dev/null || MISSING+=("pkg-config")

if ! pkg-config --exists libpipewire-0.3 2>/dev/null; then
    MISSING+=("libpipewire-0.3 (pipewire headers)")
fi
if ! pkg-config --exists libspa-0.2 2>/dev/null; then
    MISSING+=("libspa-0.2 (pipewire SPA headers)")
fi
if ! pkg-config --exists libsystemd 2>/dev/null; then
    MISSING+=("libsystemd (systemd-libs)")
fi

if [ ${#MISSING[@]} -gt 0 ]; then
    die "Missing dependencies: ${MISSING[*]}

  Arch/CachyOS:  sudo pacman -S meson ninja pipewire libpipewire systemd-libs pkg-config
  Fedora:        sudo dnf install meson ninja-build pipewire-devel systemd-devel
  Ubuntu/Debian: sudo apt install meson ninja-build libpipewire-0.3-dev libspa-0.2-dev libsystemd-dev"
fi

info "All dependencies found."

# ---- Check SDK ----
if [ ! -f /opt/maxine-afx/nvafx/lib/libnv_audiofx.so ] && \
   ! ldconfig -p 2>/dev/null | grep -q libnv_audiofx; then
    warn "NVIDIA Maxine AFX SDK not found."
    echo "  The plugin will build but won't work without the SDK."
    echo "  Run: ./scripts/setup-sdk.sh YOUR_NGC_API_KEY"
    echo ""
fi

# ---- Clean build dir if requested ----
if $CLEAN && [ -d "$BUILD_DIR" ]; then
    info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# ---- Configure ----
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    info "Configuring build (prefix=$PREFIX)..."
    meson setup "$BUILD_DIR" --prefix="$PREFIX"
else
    info "Build already configured. Reconfiguring prefix..."
    meson configure "$BUILD_DIR" --prefix="$PREFIX"
fi

# ---- Build ----
info "Building..."
meson compile -C "$BUILD_DIR"

# ---- Install ----
info "Installing (may require sudo)..."
sudo meson install -C "$BUILD_DIR"

# ---- User config directory ----
PIPEWIRE_CONF_DIR="$HOME/.config/pipewire/pipewire.conf.d"
MAXINE_CONF_DIR="$HOME/.config/maxine-pipewire"

info "Setting up user configuration..."

mkdir -p "$PIPEWIRE_CONF_DIR"
mkdir -p "$MAXINE_CONF_DIR"

# Copy PipeWire config if not already present
if [ ! -f "$PIPEWIRE_CONF_DIR/60-maxine-echo-cancel.conf" ]; then
    cp "$PROJECT_DIR/config/60-maxine-echo-cancel.conf" "$PIPEWIRE_CONF_DIR/"
    info "PipeWire config copied to $PIPEWIRE_CONF_DIR/"
else
    info "PipeWire config already exists, skipping."
fi

# Create default daemon config if not present
if [ ! -f "$MAXINE_CONF_DIR/config.toml" ]; then
    cat > "$MAXINE_CONF_DIR/config.toml" <<'TOML'
# Maxine PipeWire configuration
# See: https://github.com/darudas/maxine-pipewire

[sdk]
model_path = "/opt/maxine-afx/features"
# sdk_lib_path = "/opt/maxine-afx/nvafx/lib"

[audio]
sample_rate = 48000
default_intensity = 1.0
effects = ["denoiser"]

[audio.denoiser]
enabled = true
intensity = 0.8

[daemon]
log_level = "info"
TOML
    info "Default config written to $MAXINE_CONF_DIR/config.toml"
else
    info "Daemon config already exists, skipping."
fi

# ---- Enable systemd service ----
if $ENABLE_SERVICE; then
    info "Enabling systemd user service..."
    systemctl --user daemon-reload
    systemctl --user enable maxined.service 2>/dev/null || \
        warn "Could not enable maxined.service (service file may not be installed yet)"
    echo "  Start with: systemctl --user start maxined"
fi

# ---- Summary ----
echo ""
echo -e "${BOLD}=== Installation complete ===${NC}"
echo ""
echo "  Installed:"
echo "    maxined           $(command -v maxined 2>/dev/null || echo "$PREFIX/bin/maxined")"
echo "    maxctl            $(command -v maxctl 2>/dev/null || echo "$PREFIX/bin/maxctl")"
echo "    SPA plugin        $(pkg-config --variable=plugindir libspa-0.2)/aec/libspa-aec-maxine.so"
echo "    PipeWire config   $PIPEWIRE_CONF_DIR/60-maxine-echo-cancel.conf"
echo "    Daemon config     $MAXINE_CONF_DIR/config.toml"
echo ""
echo "  Usage (SPA plugin mode):"
echo "    systemctl --user restart pipewire"
echo "    # Select 'Maxine Clean Mic' as input in your app"
echo ""
echo "  Usage (daemon mode):"
echo "    systemctl --user start maxined"
echo "    maxctl status"
echo "    maxctl enable denoiser"
echo "    maxctl set denoiser intensity 0.8"
echo ""
