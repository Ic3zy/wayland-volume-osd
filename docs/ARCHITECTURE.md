# Sound OSD - Architecture & Protocol Design

This document details the internal architecture and Wayland protocol implementation of `sound_osd`.

## Architectural Overview

```
                      +-------------------------+
                      |   Application / Demo    |
                      +-------------------------+
                                   |
                                   v
                      +-------------------------+
                      |      Public API         |
                      |       (osd.c)           |
                      +-------------------------+
                                   |
           +-----------------------+-----------------------+
           |                       |                       |
           v                       v                       v
+--------------------+   +--------------------+   +--------------------+
|  Wayland Core &    |   |   Backend Layer    |   | Cairo Renderer &   |
|  Registry Listener |   | (layer_shell / xdg)|   | Frame Animation    |
|   (wayland.c)      |   | (layer_shell.c /   |   | (render.c /        |
|                    |   |  xdg_backend.c)    |   |  animation.c)      |
+--------------------+   +--------------------+   +--------------------+
           |                       |                       |
           +-----------------------+-----------------------+
                                   |
                                   v
                      +-------------------------+
                      | Wayland Client Protocol |
                      |    (libwayland-client)  |
                      +-------------------------+
```

## Backend Resolution Strategy

`sound_osd` never checks compositor names (such as KWin, Hyprland, Sway, or Mutter). Instead, capability detection is performed dynamically during Wayland registry enumeration:

1. **Preferred Backend (`zwlr_layer_shell_v1`):**
   - Binds to `zwlr_layer_shell_v1`.
   - Assigns surface role on `ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY`.
   - Sets `keyboard_interactivity = 0` (no keyboard focus).
   - Sets `exclusive_zone = 0` (no window layout displacement).
   - Applies position anchors and margins.

2. **Fallback Backend (`xdg_wm_base`):**
   - Used automatically when `zwlr_layer_shell_v1` is unavailable.
   - Binds to `xdg_wm_base`.
   - Creates a borderless, decoration-free `xdg_toplevel` / `xdg_surface`.
   - Acknowledges compositor configure events without focus grab.

## Shared Memory (SHM) Management

To achieve low-latency rendering without external toolkit overhead:

- **Anonymous File Descriptors:** Created using `memfd_create(2)` with `MFD_CLOEXEC` on modern Linux kernels, falling back to `mkstemp(3)` / `shm_open(3)` on POSIX systems.
- **Double Buffering:** Allocates two ARGB8888 `wl_buffer` instances. When one buffer is attached to the Wayland surface, the other remains free for Cairo rendering. Buffer lifecycle is managed via `wl_buffer.release` events.
- **DPI Scaling:** `wl_output.scale` is tracked to multiply pixel buffer dimensions and Cairo canvas scaling accordingly, preventing pixelation on High-DPI displays.

## Animation Engine

- Driven strictly by Wayland frame callbacks (`wl_surface.frame`).
- Calculates non-linear easing transitions (Cubic Ease-Out) for smooth alpha fading and vertical slide offsets.
- Handles Wayland compositor frame throttling by enforcing non-zero initial alpha commits during fade-in transitions.
