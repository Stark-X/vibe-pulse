# Pulse

AI agent monitor — Qt6 + QML floating overlay for Hyprland/Wayland.

Monitors Claude Code, Codex, OpenCode processes; shows session name, current step, and click-to-focus.

## Dependencies

```bash
sudo apt install \
  qt6-base-dev qt6-base-private-dev \
  qt6-declarative-dev qt6-wayland-dev \
  qml6-module-qtquick qml6-module-qtquick-window \
  wayland-protocols
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Binaries: `build/pulse`, `build/pulse-toggle`

## Hyprland setup

Add to `~/.config/hypr/hyprland.conf`:

```ini
# Toggle Pulse overlay with Super+P
bind = SUPER, P, exec, /path/to/pulse-toggle
```

## Running

```bash
./build/pulse &
```

The overlay appears top-right, transparent, always-on-top. No taskbar entry.

## Themes

Three built-in themes: `midnight` (default), `aurora`, `carbon`.  
Settings persist in `~/.config/Pulse/Pulse.conf`.
