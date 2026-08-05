# Sound OSD - API Reference

The `libosd` library provides a minimalist C17 API for displaying volume On-Screen Displays (OSD) natively on Wayland compositors without toolkit overhead.

## Header File

```c
#include "osd.h"
```

## Data Structures

### `osd_position_t`

Defines the screen position for the OSD overlay.

```c
typedef enum osd_position {
    OSD_POS_CENTER = 0,
    OSD_POS_TOP_CENTER,
    OSD_POS_BOTTOM_CENTER,
    OSD_POS_TOP_LEFT,
    OSD_POS_TOP_RIGHT,
    OSD_POS_BOTTOM_LEFT,
    OSD_POS_BOTTOM_RIGHT,
} osd_position_t;
```

### `osd_color_t`

Defines RGBA normalized color values (0.0 to 1.0).

```c
typedef struct osd_color {
    double r;
    double g;
    double b;
    double a;
} osd_color_t;
```

### `osd_config_t`

Configuration properties controlling OSD geometry, colors, and animation timeouts.

```c
typedef struct osd_config {
    int width;                  /* Width in pixels (default: 200) */
    int height;                 /* Height in pixels (default: 200) */
    int corner_radius;          /* Corner radius (default: 20) */
    osd_position_t position;    /* Screen placement (default: OSD_POS_BOTTOM_CENTER) */
    int margin_x;               /* Horizontal margin offset */
    int margin_y;               /* Vertical margin offset */
    
    osd_color_t bg_color;       /* Card background color */
    osd_color_t fg_color;       /* Progress arc fill color */
    osd_color_t track_color;    /* Progress arc track background */
    osd_color_t icon_color;     /* Vector icon and text color */
    
    uint32_t animation_ms;      /* Fade/Slide animation duration in ms (default: 200) */
    uint32_t timeout_ms;        /* Auto-hide duration after trigger in ms (default: 2000) */
} osd_config_t;
```

## Functions

### `osd_init`

```c
bool osd_init(void);
```

Initializes the Wayland display connection, registry listeners, backend capabilities (`zwlr_layer_shell_v1` or `xdg_wm_base`), and SHM rendering context.

- **Returns:** `true` on success, `false` on initialization error.

---

### `osd_set_config`

```c
void osd_set_config(const osd_config_t *config);
```

Applies custom configuration options to the OSD context. Can be called at any time to update positioning or styling.

- **Parameters:**
  - `config`: Pointer to the `osd_config_t` structure.

---

### `osd_get_config`

```c
void osd_get_config(osd_config_t *config);
```

Retrieves the current active configuration.

- **Parameters:**
  - `config`: Pointer to target `osd_config_t` structure to populate.

---

### `osd_show_volume`

```c
void osd_show_volume(int volume, bool muted);
```

Displays the volume OSD with the specified volume percentage and mute status. If the OSD is currently visible, calling this function updates the volume level and resets the auto-hide timer (`timeout_ms`).

- **Parameters:**
  - `volume`: Volume level percentage (0 to 100).
  - `muted`: `true` if audio is muted, `false` otherwise.

---

### `osd_hide`

```c
void osd_hide(void);
```

Triggers the fade-out animation and hides the OSD immediately.

---

### `osd_dispatch`

```c
void osd_dispatch(int timeout_ms);
```

Processes pending Wayland display events with a poll timeout in milliseconds. Call this periodically or inside your main loop to drive frame callbacks and timers.

- **Parameters:**
  - `timeout_ms`: Poll timeout in milliseconds.

---

### `osd_delay_ms`

```c
void osd_delay_ms(int ms);
```

Utility delay function that actively dispatches Wayland animation frames for the specified duration.

- **Parameters:**
  - `ms`: Delay duration in milliseconds.

---

### `osd_destroy`

```c
void osd_destroy(void);
```

Destroys all Wayland surfaces, buffers, frame callbacks, and closes the Wayland display connection.
