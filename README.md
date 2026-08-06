# Sound OSD

A native, low-latency Wayland On-Screen Display (OSD) library written in pure C17.

`sound_osd` provides a lightweight, modular volume OSD designed for modern Linux desktop environments. It operates directly on top of the native Wayland client protocol without relying on heavy GUI toolkits such as Qt, GTK, SDL, GLFW, or Electron.

## Features

- **Pure C17 & Wayland Native:** Uses `libwayland-client` directly for low latency, low memory footprint, and high performance.
- **Toolkit-Free:** Zero dependencies on Qt, GTK, SDL, Electron, or wxWidgets.
- **Zero-Blocking Execution (0ms Overhead):** `osd_show_volume()` updates state and returns instantly (<0.05ms) using a background POSIX thread (`pthread`) with fine-grained mutex synchronization.
- **100% Click-Through Pass-Through:** Sets an empty `wl_region` input mask, passing all mouse clicks, hovers, and touch events through to underlying windows.
- **Complete Compositor Shadow Destruction:** Completely destroys surface nodes when hidden, guaranteeing 100% elimination of residual shadows or ghosting on KWin and Mutter.
- **Configurable Max Volume & Overamplification:** Supports volume scaling up to 150%+ with dynamic warm amber/crimson gradient transitions for overamplified audio (>100%).
- **Dynamic Vector Speaker Icon:** Adaptive speaker vector icon displaying 0 waves for 0%/muted, 1 wave for <= 50%, 2 waves for > 50%, and a crimson slash for muted state.
- **Dynamic Capability Detection:** Preferred support for `zwlr_layer_shell_v1` on the `overlay` layer. Automatically falls back to borderless `xdg_wm_base` surfaces without hardcoded compositor checks.
- **Frame-Callback Driven Animations:** Custom Fade-in, Fade-out, and Slide animations driven strictly by `wl_surface.frame` callbacks with cubic ease-out transitions.
- **Cross-Linux Compatibility:** Portable POSIX / Linux shared memory (SHM) buffer management (`memfd_create` with fallback to `mkstemp`/`shm_open`) supporting glibc, musl, Alpine, Arch, Ubuntu, Fedora, and NixOS.

## Quick Start

### Build Prerequisites

Ensure you have a C17 compiler, `make`, `pkg-config`, `libwayland-client`, `wayland-scanner`, `libcairo`, and `libpthread` installed.

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
    /* Initialize Wayland connection, renderer, and background worker thread */
    if (!osd_init()) {
        return 1;
    }

    /* Configure maximum volume scale up to 150% */
    osd_config_t config;
    osd_get_config(&config);
    config.max_volume = 150;
    osd_set_config(&config);

    /* Display volume at 135% with 0ms blocking */
    osd_show_volume(135, false);

    /* Allow background thread to process animation for 2 seconds */
    osd_delay_ms(2000);

    /* Clean up all Wayland resources */
    osd_destroy();
    return 0;
}
```

For detailed API documentation, see [docs/API.md](docs/API.md).

## AI Development Attribution

This project was architected, implemented, and refined with AI pair-programming assistance by **Google Antigravity AI assistant**, powered by **Gemini 3.6 Flash** models.

## License

This project is open-source and available under the [MIT License](LICENSE).
