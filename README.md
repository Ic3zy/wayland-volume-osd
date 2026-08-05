# Sound OSD

A native, low-latency Wayland On-Screen Display (OSD) library written in pure C17.

`sound_osd` provides a lightweight, modular volume OSD designed for modern Linux desktop environments. It operates directly on top of the native Wayland client protocol without relying on heavy GUI toolkits such as Qt, GTK, SDL, GLFW, or Electron.

## Features

- **Pure C17 & Wayland Native:** Uses `libwayland-client` directly for low latency, low memory footprint, and high performance.
- **Toolkit-Free:** Zero dependencies on Qt, GTK, SDL, Electron, or wxWidgets.
- **Dynamic Capability Detection:** Preferred support for `zwlr_layer_shell_v1` on the `overlay` layer. Automatically falls back to borderless `xdg_wm_base` surfaces on compositors lacking layer-shell support without hardcoded compositor checks.
- **Always-on-Top & Unfocused:** Never steals keyboard focus and never appears in task switchers or Alt-Tab menus.
- **Cairo Vector Rendering:** Premium circular card design featuring smooth glassmorphism dark backgrounds, radial ambient glow, high-DPI scaling support, and a 270-degree progress ring.
- **Dynamic Vector Speaker Icon:** Vector icon that dynamically adapts sound wave arcs (1 wave for low volume <= 50%, 2 waves for high volume > 50%, and a dedicated crimson slash for muted state).
- **Frame-Callback Driven Animations:** Custom Fade-in, Fade-out, and Slide animations driven strictly by `wl_surface.frame` callbacks with non-linear cubic ease-out transitions.
- **Auto-Hide & Re-Triggering:** Showing the OSD again while visible automatically resets the hide timer.
- **Cross-Linux Compatibility:** Portable POSIX / Linux shared memory (SHM) buffer management (`memfd_create` with fallback to `mkstemp`/`shm_open`) supporting glibc, musl, Alpine, Arch, Ubuntu, Fedora, and NixOS.

## Architecture

The project is structured into modular components:

```
sound_osd/
├── include/
│   └── osd.h                  # Public C API header
├── src/
│   ├── wayland.h / .c         # Display connection, registry, output scale, & SHM
│   ├── layer_shell.h / .c     # zwlr_layer_shell_v1 preferred overlay backend
│   ├── xdg_backend.h / .c     # xdg_wm_base fallback borderless backend
│   ├── render.h / .c          # Cairo ARGB32 rendering & circular UI layout
│   ├── animation.h / .c       # Frame-callback driven animation engine
│   └── osd.c                  # Core state machine, auto-hide timer, & public API
├── protocols/
│   ├── wlr-layer-shell-unstable-v1.xml
│   └── xdg-shell.xml
├── docs/
│   ├── API.md                 # Full public C API documentation
│   ├── ARCHITECTURE.md        # Technical architectural breakdown
│   └── BUILDING.md           # Compilation guide across distributions
├── demo.c                     # Demonstration program
├── test_phase1.c              # Wayland protocol verification test
└── Makefile                   # Build script with wayland-scanner generation
```

## Quick Start

### Build Prerequisites

Ensure you have a C17 compiler, `make`, `pkg-config`, `libwayland-client`, `wayland-scanner`, and `libcairo` installed.

For detailed distribution-specific package commands, see [docs/BUILDING.md](docs/BUILDING.md).

### Compilation

```bash
make clean && make
```

This generates `libosd.so` (shared library), `libosd.a` (static library), and the `demo` binary.

### Running the Demo

```bash
./demo
```

## Basic API Usage

```c
#include "osd.h"

int main(void)
{
    /* Initialize Wayland connection and select backend */
    if (!osd_init()) {
        return 1;
    }

    /* Display volume at 75% */
    osd_show_volume(75, false);

    /* Process Wayland animation frames for 2 seconds */
    osd_delay_ms(2000);

    /* Clean up all Wayland resources */
    osd_destroy();
    return 0;
}
```

For detailed API documentation, see [docs/API.md](docs/API.md).

## Configuration

Customize position, geometry, colors, and animation duration via `osd_config_t`:

```c
osd_config_t config;
osd_get_config(&config);

config.position = OSD_POS_BOTTOM_CENTER;
config.margin_y = 100;
config.animation_ms = 200;
config.timeout_ms = 2000;

osd_set_config(&config);
```

## AI Development Attribution

This project was architected, implemented, and refined with AI pair-programming assistance by **Google Antigravity AI assistant**, powered by **Gemini 3.6 Flash** models.

## License

This project is open-source and available under the [MIT License](LICENSE).
