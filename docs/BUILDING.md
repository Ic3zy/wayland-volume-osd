# Sound OSD - Building & Installation Guide

This document covers build requirements and step-by-step instructions for compiling `sound_osd` across different Linux distributions.

## Prerequisites

### Required Dependencies

- C17 compliant compiler (`gcc` or `clang`)
- `make`
- `pkg-config`
- `libwayland-client`
- `wayland-scanner`
- `libcairo`

### Installing Dependencies by Distribution

#### Arch Linux
```bash
sudo pacman -S base-devel wayland wayland-protocols cairo pkgconf
```

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install build-essential libwayland-dev wayland-protocols libcairo2-dev pkg-config
```

#### Fedora
```bash
sudo dnf install gcc make wayland-devel wayland-protocols-devel cairo-devel pkgconf-pkg-config
```

#### Alpine Linux
```bash
sudo apk add build-base wayland-dev wayland-protocols cairo-dev pkgconf
```

#### NixOS
```nix
environment.systemPackages = with pkgs; [
  gcc
  gnumake
  pkg-config
  wayland
  wayland-protocols
  cairo
];
```

## Compilation

Clone the repository and run `make`:

```bash
git clone https://github.com/username/sound_osd.git
cd sound_osd
make clean && make
```

### Build Outputs

The build process generates the following artifacts:

- `libosd.so`: Dynamic shared library
- `libosd.a`: Static library for direct embedding
- `demo`: Interactive demonstration executable

## Running the Demo

```bash
./demo
```
