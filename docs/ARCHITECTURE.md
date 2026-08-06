# Sound OSD - Architecture & Protocol Design

This document details the internal architecture, thread model, input handling, and Wayland protocol implementation of `sound_osd`.

## Architectural Overview

```
                      +-------------------------+
                      | Application / Python UI |
                      +-------------------------+
                                   |
            (0ms Non-Blocking)     v
                      +-------------------------+
                      |      Public API         |
                      |       (osd.c)           |
                      +-------------------------+
                                   |
         +-------------------------+-------------------------+
         | (Fine-Grained Mutex)                              | (Background Loop)
         v                                                   v
+--------------------+                     +--------------------+
| Wayland Core &     |                     | Background Worker  |
| Backend Resolution |                     | Dispatch Thread    |
| (wayland.c)        |                     | (pthread)          |
+--------------------+                     +--------------------+
         |                                                   |
         +-------------------------+-------------------------+
                                   |
                                   v
                      +-------------------------+
                      | Cairo Renderer Engine & |
                      | Frame Animation         |
                      | (render.c / anim.c)     |
                      +-------------------------+
                                   |
                                   v
                      +-------------------------+
                      | Wayland Client Protocol |
                      |   (libwayland-client)   |
                      +-------------------------+
```

## Non-Blocking Asynchronous Threading Model

`sound_osd` implements a multi-threaded architecture using POSIX threads (`pthread`) and fine-grained mutex synchronization:

- **0ms Blocking Execution:** Calls to `osd_show_volume()` update state variables and flush protocol buffers under a microsecond mutex lock, returning control to the caller instantly (<0.05ms execution time).
- **Background Dispatch Worker:** A dedicated `pthread` executes socket polling (`poll`) **without holding the mutex**, ensuring the main thread is never blocked during socket wait or auto-hide timer intervals.
- **Asynchronous Surface Lifecycle:** Surfaces are initialized asynchronously without synchronous `wl_display_roundtrip` blocks.

## 100% Click-Through Input Pass-Through

- An empty Wayland region (`struct wl_region *empty_region = wl_compositor_create_region(...)`) is explicitly assigned to `wl_surface_set_input_region()`.
- Tells the Wayland compositor that the OSD surface has no interactive input bounds.
- All mouse clicks, pointer hovers, touch gestures, and tablet inputs pass 100% through to underlying applications.

## Complete Compositor Shadow Cache Purging

- When the animation reaches `OSD_ANIM_HIDDEN`, `destroy_osd_surface_nodes()` destroys both the layer surface (`zwlr_layer_surface_v1_destroy`) and Wayland surface (`wl_surface_destroy`).
- Completely unlinks the node from the Wayland compositor's scene graph, guaranteeing 100% elimination of residual shadows, blur artifacts, or ghosting on KWin and Mutter.
- Re-creates the surface instantly on the next `osd_show_volume()` trigger.

## Renderer & Overamplification Styling

- **Circular Progress Arc:** Draws a 270-degree arc (135° to 405°) based on `volume / max_volume`.
- **Dynamic Overamplification Gradient:** When `volume > 100%` (e.g. up to `max_volume = 150`), the arc and typography automatically transition to a warm amber to crimson warning gradient (`#F59E0B` -> `#EF4444`).
- **Adaptive Vector Icon:** Draws 0 waves for 0% / Muted, 1 wave for <= 50%, 2 waves for > 50%, and a crimson diagonal slash for muted state.
