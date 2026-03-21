# Maxine PipeWire

**NVIDIA Broadcast for Linux**

Maxine PipeWire brings NVIDIA's Maxine Audio/Video Effects SDK to Linux desktops
through native PipeWire integration. It provides GPU-accelerated noise removal,
echo cancellation, room echo removal, super-resolution upsampling, and more --
all running on your RTX GPU's Tensor Cores and controllable in real time.

Unlike NVIDIA Broadcast on Windows, which offers simple on/off toggles, Maxine
PipeWire gives you continuous intensity sliders (0.0--1.0), chainable effects,
per-effect enable/disable, and a D-Bus control interface so scripts and desktop
widgets can drive everything programmatically.

## Feature Comparison

| Feature | NVIDIA Broadcast (Windows) | Maxine PipeWire (Linux) |
|---|---|---|
| Noise Removal | On/Off only | 0.0--1.0 intensity slider |
| Echo Cancellation | On/Off only | 0.0--1.0 intensity slider |
| Room Echo Removal (Dereverb) | Not available | 0.0--1.0 intensity slider |
| Combined Dereverb+Denoise | Not available | Single chained effect |
| Super-Resolution (8k/16k to 48k) | Not available | Chained upsampling |
| Speaker Focus | Basic | With intensity control |
| Studio Voice (HQ / Low Latency) | Not available | Two quality presets |
| Voice Font | Not available | Reference-audio based |
| VAD (Voice Activity Detection) | Not available | Per-effect toggle |
| Effect Chaining | Fixed pipeline | Arbitrary chain of up to 16 effects |
| Live Parameter Changes | Requires restart | Instant via D-Bus or CLI |
| CLI Control | None | `maxctl` with full command set |
| Scriptable | No | D-Bus interface, shell-friendly CLI |
| Config Format | Registry / GUI | TOML (human-editable) |
| Integration | Proprietary driver | Native PipeWire SPA plugin + daemon |

## Supported Effects

### Audio (AFX SDK)

| Effect | ID | Sample Rates | Intensity | Description |
|---|---|---|---|---|
| Noise Removal | `denoiser` | 16 kHz, 48 kHz | Yes | Removes background noise (fans, typing, HVAC) |
| Room Echo Removal | `dereverb` | 16 kHz, 48 kHz | Yes | Reduces room reverb / hollow sound |
| Combined Denoise+Dereverb | `dereverb_denoiser` | 16 kHz, 48 kHz | Yes | Both in a single pass |
| Echo Cancellation | `aec` | 16 kHz, 48 kHz | Yes | Removes speaker feedback from mic |
| Super-Resolution | `superres` | 16 kHz | No | Upsamples narrow-band to wider bandwidth |
| Studio Voice HQ | `studio_voice_hq` | 48 kHz | No | High-quality voice enhancement |
| Studio Voice Low Latency | `studio_voice_ll` | 48 kHz | No | Lower-latency voice enhancement |
| Speaker Focus | `speaker_focus` | 16 kHz, 48 kHz | Yes | Isolates primary speaker, attenuates others |
| Voice Font | `voice_font_hq` / `voice_font_ll` | 48 kHz | No | Voice conversion using reference audio |

### Chained Effects

| Chain | ID | Description |
|---|---|---|
| Denoise + Super-Res | `denoiser16k_superres16kto48k` | Denoise at 16 kHz, upsample to 48 kHz |
| Dereverb + Super-Res | `dereverb16k_superres16kto48k` | Dereverb at 16 kHz, upsample to 48 kHz |
| Dereverb+Denoise + Super-Res | `dereverb_denoiser16k_superres16kto48k` | Full cleanup + upsample |
| Super-Res + Denoise | `superres8kto16k_denoiser16k` | Upsample 8 kHz to 16 kHz, then denoise |
| Super-Res + Dereverb | `superres8kto16k_dereverb16k` | Upsample 8 kHz to 16 kHz, then dereverb |
| Super-Res + Dereverb+Denoise | `superres8kto16k_dereverb_denoiser16k` | Upsample + full cleanup |
| Denoise + Speaker Focus (16k) | `denoiser16k_speaker_focus16k` | Denoise then isolate speaker |
| Denoise + Speaker Focus (48k) | `denoiser48k_speaker_focus48k` | Denoise then isolate speaker (wideband) |

## Requirements

### Hardware

- NVIDIA GPU with Tensor Cores: **RTX 2060** or newer (RTX 20xx, 30xx, 40xx, 50xx series)
- Minimum 4 GB VRAM (6+ GB recommended when stacking effects)

### Software

- Linux with PipeWire (0.3.x or later)
- NVIDIA driver **525.60** or newer (535+ recommended)
- NVIDIA Maxine AFX SDK (downloaded separately; see below)
- `meson` and `ninja` build tools
- `libpipewire-0.3` and `libspa-0.2` development headers
- `libsystemd` development headers (for daemon and CLI)

### Arch Linux / CachyOS

```bash
sudo pacman -S pipewire libpipewire meson ninja systemd-libs
```

## Quick Start

```bash
# 1. Download the Maxine AFX SDK
./scripts/setup-sdk.sh YOUR_NGC_API_KEY

# 2. Build
meson setup build
meson compile -C build

# 3. Install
sudo meson install -C build

# 4. Copy PipeWire config (for SPA echo-cancel plugin mode)
mkdir -p ~/.config/pipewire/pipewire.conf.d
cp config/60-maxine-echo-cancel.conf ~/.config/pipewire/pipewire.conf.d/

# 5. Restart PipeWire
systemctl --user restart pipewire

# --- OR use the daemon mode ---

# 5b. Enable and start the daemon
systemctl --user enable --now maxined

# 6. Control effects
maxctl status
maxctl enable denoiser
maxctl set denoiser intensity 0.8
```

## Installation

### Step 1: Obtain the NVIDIA Maxine AFX SDK

The SDK is freely available but requires an NVIDIA NGC account.

1. Create a free account at <https://catalog.ngc.nvidia.com/>
2. Generate an API key at <https://org.ngc.nvidia.com/setup/api-key>
3. Run the setup script:

```bash
./scripts/setup-sdk.sh YOUR_NGC_API_KEY
```

This downloads the SDK to `/opt/maxine-afx/` and configures library paths.

To also install the Video Effects SDK (for future VFX support):

```bash
./scripts/setup-sdk.sh --vfx YOUR_NGC_API_KEY
```

### Step 2: Build

```bash
meson setup build
meson compile -C build
```

To enable video effects support (requires VFX SDK):

```bash
meson setup build -Dvideo_effects=true
meson compile -C build
```

### Step 3: Install

```bash
sudo meson install -C build
```

This installs:

| What | Where |
|---|---|
| `libmaxine-core.so` | `/usr/local/lib/` |
| `libspa-aec-maxine.so` | `/usr/lib/spa-0.2/aec/` |
| `maxined` | `/usr/local/bin/` |
| `maxctl` | `/usr/local/bin/` |
| Config template | `/usr/local/etc/maxine-pipewire/` |
| Systemd service | systemd user unit directory |
| D-Bus service | `/usr/local/share/dbus-1/services/` |

### Step 4: Choose Your Mode

Maxine PipeWire supports two modes:

**A) SPA Plugin Mode** -- integrates directly into PipeWire's echo cancellation module.
Best for simple "just clean up my mic" setups.

```bash
mkdir -p ~/.config/pipewire/pipewire.conf.d
cp /usr/local/etc/maxine-pipewire/60-maxine-echo-cancel.conf \
   ~/.config/pipewire/pipewire.conf.d/
systemctl --user restart pipewire
```

**B) Daemon Mode** -- runs `maxined` as a user service with D-Bus control.
Best for dynamic effect chaining, live parameter tuning, and scripting.

```bash
systemctl --user enable --now maxined
```

## CLI Usage (`maxctl`)

`maxctl` communicates with the `maxined` daemon over D-Bus.

### View status

```bash
maxctl status
```

Shows all loaded effects, their enabled state, intensity, and processing stats.

### List available effects

```bash
maxctl list
```

### Enable / disable an effect

```bash
maxctl enable denoiser
maxctl disable denoiser
```

### Set effect intensity

```bash
maxctl set denoiser intensity 0.7
```

### Toggle VAD (voice activity detection)

```bash
maxctl set denoiser vad on
maxctl set denoiser vad off
```

### Add / remove effects from the chain

```bash
maxctl add dereverb
maxctl add denoiser
maxctl remove dereverb
```

### Reload configuration

```bash
maxctl reload
```

### View daemon version and GPU info

```bash
maxctl info
```

### Compact one-liners for scripts

```bash
# Mute-on-push: disable all effects while key held
maxctl disable-all
maxctl enable-all

# Quick gaming preset
maxctl set denoiser intensity 1.0 && maxctl enable denoiser && maxctl disable dereverb
```

## Configuration

The daemon reads a TOML configuration file at `~/.config/maxine-pipewire/config.toml`
(falls back to `/usr/local/etc/maxine-pipewire/config.toml`).

```toml
# Maxine PipeWire configuration

[sdk]
# Path to the Maxine AFX features directory
model_path = "/opt/maxine-afx/features"

# Path to libnv_audiofx.so (leave empty to use LD_LIBRARY_PATH / ldconfig)
# sdk_lib_path = "/opt/maxine-afx/nvafx/lib"

[audio]
# Sample rate: 16000 or 48000
sample_rate = 48000

# Default intensity for all effects (0.0 - 1.0)
default_intensity = 1.0

# Effects to load at startup (order matters -- processed in sequence)
effects = ["denoiser"]

[audio.denoiser]
enabled = true
intensity = 0.8

[audio.dereverb]
enabled = false
intensity = 0.6

[audio.aec]
enabled = false
intensity = 1.0
denoise = true     # run denoiser after AEC

[audio.speaker_focus]
enabled = false
intensity = 0.7

[daemon]
# Log level: error, warn, info, debug, trace
log_level = "info"

# D-Bus interface name
dbus_name = "com.github.maxine_pipewire.Daemon"
```

### Configuration Keys Reference

| Section | Key | Type | Default | Description |
|---|---|---|---|---|
| `sdk` | `model_path` | string | `/opt/maxine-afx/features` | Path to SDK feature models |
| `sdk` | `sdk_lib_path` | string | (system) | Path to `libnv_audiofx.so` |
| `audio` | `sample_rate` | int | `48000` | Global sample rate (16000 or 48000) |
| `audio` | `default_intensity` | float | `1.0` | Fallback intensity for effects |
| `audio` | `effects` | list | `["denoiser"]` | Effects loaded at startup |
| `audio.<effect>` | `enabled` | bool | `false` | Whether effect is active |
| `audio.<effect>` | `intensity` | float | `1.0` | Processing strength 0.0--1.0 |
| `audio.<effect>` | `vad` | bool | `false` | Voice activity detection |
| `daemon` | `log_level` | string | `"info"` | Logging verbosity |
| `daemon` | `dbus_name` | string | (builtin) | D-Bus bus name |

## Architecture

```
                  +-----------+
                  |  maxctl   |  CLI tool
                  +-----+-----+
                        | D-Bus (sd-bus)
                  +-----v-----+
                  |  maxined   |  Daemon (PipeWire client)
                  +-----+-----+
                        |
            +-----------+-----------+
            |                       |
      +-----v-----+          +-----v-----+
      | audio_node |  . . .  | audio_node |  pw_filter per effect
      | (denoiser) |         |  (dereverb)|
      +-----+------+         +-----+------+
            |                       |
      +-----v-----------------------v-----+
      |        libmaxine-core.so          |
      |  maxine_loader  effect_registry   |
      |  maxine_config                    |
      +-----+-----------------------------+
            | dlopen
      +-----v-----------+
      | libnv_audiofx.so |  NVIDIA Maxine AFX SDK
      +-----------------+

  Alternatively, the SPA plugin (libspa-aec-maxine.so) can be loaded
  directly by PipeWire's module-echo-cancel without the daemon.
```

- **libmaxine-core.so** -- dynamically loads the NVIDIA SDK via `dlopen`, manages the effect
  registry, and parses configuration. No compile-time dependency on the SDK.
- **maxined** -- a PipeWire client daemon that creates `pw_filter` nodes for each effect,
  wired in a configurable chain. Exposes a D-Bus interface for runtime control.
- **maxctl** -- a lightweight CLI that talks to the daemon over D-Bus (`sd-bus`).
- **libspa-aec-maxine.so** -- a standalone SPA plugin for PipeWire's built-in echo-cancel
  module. Simpler setup, but less flexible than the daemon.

## Troubleshooting

### "failed to load libnv_audiofx.so"

The SDK library is not in the linker search path.

```bash
# Check if ldconfig knows about it
ldconfig -p | grep nv_audiofx

# If missing, create config
echo "/opt/maxine-afx/nvafx/lib" | sudo tee /etc/ld.so.conf.d/maxine-afx.conf
sudo ldconfig
```

### "GPU unsupported (no tensor cores?)"

You need an RTX 2060 or newer. GTX cards do not have Tensor Cores and cannot run
Maxine effects.

```bash
# Check your GPU compute capability
nvidia-smi --query-gpu=name,compute_cap --format=csv,noheader
# Needs compute capability >= 7.5 (Turing) for AFX
```

### "model load failed"

The model files for your GPU architecture are missing.

```bash
# Re-run SDK setup to download models
./scripts/setup-sdk.sh --gpu l4 YOUR_NGC_API_KEY

# Verify model directory exists
ls /opt/maxine-afx/features/denoiser/models/sm_89/
```

### "unsupported sample rate"

Maxine only supports 16 kHz and 48 kHz. Make sure your PipeWire config or daemon
config uses one of these rates.

### No virtual device appears in audio settings

```bash
# Check if PipeWire loaded the module
pw-cli ls Node | grep -i maxine

# Check PipeWire logs
journalctl --user -u pipewire -n 50 --no-pager | grep -i maxine
```

### High latency or audio glitches

- Lower the effect count -- each effect adds one frame of latency (~10 ms at 48 kHz).
- Ensure the NVIDIA driver is not in power-save mode:
  ```bash
  nvidia-smi -pm 1   # persistent mode
  ```
- Check GPU load:
  ```bash
  nvidia-smi dmon -d 1
  ```

### Daemon won't start

```bash
# Check logs
journalctl --user -u maxined -n 30 --no-pager

# Start manually for verbose output
maxined  # prints to stderr
```

## Contributing

Contributions are welcome. Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-change`)
3. Follow the existing code style (run `clang-format` with the included `.clang-format`)
4. Test with at least the denoiser effect on a real GPU
5. Submit a pull request with a clear description

### Code Style

- C11 (`gnu11`), 4-space indentation, no tabs
- Linux-style braces (opening brace on same line for functions is an exception in the SPA boilerplate)
- Pointer star attached to the variable name: `int *ptr`
- All public symbols prefixed with `maxine_` or `nvafx_`
- Comments: `//` for inline, block comments for file headers

### Project Structure

```
include/           Public headers
src/               Source files
config/            PipeWire config templates, systemd service
scripts/           Setup and installation scripts
doc/               Extended documentation
```

## License

MIT License. See [LICENSE](LICENSE) for details.

The NVIDIA Maxine AFX/VFX SDK is a separate product with its own license.
This project does not bundle or redistribute any NVIDIA binaries.
