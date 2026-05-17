# Pulse

AI agent monitor — Qt6 + QML floating overlay for monitoring Claude Code, Codex, and OpenCode.

Shows session name, current step, and click-to-focus. Supports Ubuntu/Hyprland (Wayland) and macOS.

## Dependencies

### macOS

```bash
brew install qt
```

### Ubuntu / Debian

```bash
sudo apt install \
  qt6-base-dev qt6-base-private-dev \
  qt6-declarative-dev qt6-wayland-dev \
  qml6-module-qtquick qml6-module-qtquick-window \
  wayland-protocols
```

## Build

```bash
make build
```

The `Makefile` auto-detects the platform and runs CMake configure on first build. No manual `cmake -B` step needed.

Binaries:
- **macOS**: `build_mac/pulse.app/Contents/MacOS/pulse`
- **Linux**: `build/pulse`

## Running

```bash
make run
```

Or run the mock preview scenes for UI development:

```bash
make mock-idle
make mock-working
make mock-permission
make mock-question
make mock-plan
make mock-expanded
```

## Hyprland setup

Add to `~/.config/hypr/hyprland.conf`:

```ini
# Toggle Pulse overlay with Super+P
bind = SUPER, P, exec, /path/to/pulse-toggle
```

## Themes

Three built-in themes: `midnight` (default), `aurora`, `carbon`.  
Settings persist in `~/.config/Pulse/Pulse.conf`.
