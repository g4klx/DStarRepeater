# DStarRepeater

A D-Star digital voice repeater controller for homebrew amateur radio hardware. DStarRepeater interfaces with [ircDDBGateway](https://github.com/g4klx/ircDDBGateway) or [DStarGateway](https://github.com/g4klx/DStarGateway) to provide D-Star networking — callsign routing, reflector linking, and ircDDB.

This is a headless CLI daemon written in portable C++17 with no GUI dependencies. It runs on Linux (x86_64, ARM/Raspberry Pi), Windows, and macOS, and has been tested on Ubuntu 24.04, Debian Trixie, and Alpine 3.21.

## Supported Hardware

| Modem | Interface |
|-------|-----------|
| DVAP Dongle | USB serial |
| DV-Mega | USB serial |
| GMSK Modem (Dutch*STAR, DUTCH-UHF) | USB (libusb) |
| Sound Card / UDRC | ALSA (Linux), PortAudio (Windows/macOS) |
| MMDVM | USB serial |
| DV-RPTR V1, V2, V3 | USB serial or network |
| Icom Terminal / Access Point Mode | USB serial |
| Split (multi-receiver) | UDP network |

## Quick Start

### 1. Install dependencies

**Ubuntu / Debian:**
```bash
sudo apt-get install g++ make libasound2-dev libusb-1.0-0-dev libmosquitto-dev
```

**Raspberry Pi (Raspberry Pi OS / Debian):**
```bash
sudo apt-get install g++ make libasound2-dev libusb-1.0-0-dev libmosquitto-dev wiringpi
```

**Alpine Linux:**
```bash
apk add g++ make alsa-lib-dev libusb-dev linux-headers mosquitto-dev
```

**Windows and macOS** — see the [platform-specific notes](#windows) below.

### 2. Build

```bash
make
```

### 3. Configure

Copy the example config and edit for your hardware:
```bash
sudo cp /etc/dstarrepeater/dstarrepeater.ini.example /etc/dstarrepeater/dstarrepeater.ini
sudo nano /etc/dstarrepeater/dstarrepeater.ini
```

See [CONFIGURATION.md](CONFIGURATION.md) for the full configuration reference.

### 4. Install and run

```bash
sudo make install
sudo dstarrepeaterd /etc/dstarrepeater/dstarrepeater.ini
```

## Build Options

By default, `make` produces a **release build** with **MQTT enabled** and **GPIO enabled on ARM** (Raspberry Pi). Any option can be overridden on the command line.

| Command | Description |
|---------|-------------|
| `make` | Default build (release, MQTT on, GPIO auto-detected) |
| `make MQTT=0` | Build without MQTT support |
| `make GPIO=0` | Build without GPIO support (even on ARM) |
| `make GPIO=1` | Force GPIO support on non-ARM platforms |
| `make BUILD=debug` | Debug build with symbols and assertions |
| `sudo make install` | Install binary, data files, and example config |
| `make clean` | Remove build artifacts |

**Defaults:**

| Option | Default | Notes |
|--------|---------|-------|
| `BUILD` | `release` | Optimised, no debug symbols |
| `MQTT` | `1` (on) | Requires `libmosquitto-dev` |
| `GPIO` | `1` on ARM, `0` otherwise | Requires `wiringPi`; auto-detected via `uname -m` |

## Dependencies

### Required (Linux)

| Package | Ubuntu/Debian | Alpine | Purpose |
|---------|---------------|--------|---------|
| C++17 compiler | `g++` (GCC 13+) | `g++` | Compilation |
| GNU Make | `make` | `make` | Build system |
| ALSA dev libraries | `libasound2-dev` | `alsa-lib-dev` | Sound card modem support |
| libusb 1.0 dev | `libusb-1.0-0-dev` | `libusb-dev` | USB modem support (GMSK) |
| Mosquitto dev | `libmosquitto-dev` | `mosquitto-dev` | MQTT telemetry (enabled by default) |
| Linux headers | — | `linux-headers` | Required on Alpine only |

### Raspberry Pi (additional)

| Package | Install | Purpose |
|---------|---------|---------|
| wiringPi | `sudo apt-get install wiringpi` | GPIO controller support (enabled by default on ARM) |

> **Note:** On newer Raspberry Pi OS releases where `wiringpi` is not in the repositories, install from the [unofficial mirror](https://github.com/WiringPi/WiringPi):
> ```bash
> git clone https://github.com/WiringPi/WiringPi.git
> cd WiringPi && ./build
> ```

### Disabling optional features

If you don't need MQTT or GPIO support, you can skip those dependencies and disable them at build time:

```bash
# Without MQTT (no libmosquitto-dev needed)
make MQTT=0

# Without GPIO on a Pi (no wiringPi needed)
make GPIO=0

# Without both
make MQTT=0 GPIO=0
```

### Windows

The codebase includes `#if defined(_WIN32)` blocks for Windows compatibility. A Windows build requires:

| Dependency | Source | Notes |
|------------|--------|-------|
| Visual Studio 2019+ or MinGW-w64 | Microsoft / MSYS2 | Must support C++17 (`/std:c++17` or `-std=c++17`) |
| libusb 1.0 | [libusb.info](https://libusb.info/) | Windows binaries available; needed for GMSK modem support |
| PortAudio | [portaudio.com](http://www.portaudio.com/) | Required for sound card modem support (replaces ALSA) |
| Eclipse Mosquitto | [mosquitto.org](https://mosquitto.org/) | Optional; only needed for MQTT builds |

The provided `Makefile` targets Linux. For Windows, you will need to create a Visual Studio project or CMakeLists.txt and link against `ws2_32.lib` (Winsock), `portaudio.lib`, and `libusb-1.0.lib`. Serial port, socket, and signal handling code uses Win32 APIs (`CreateFile`, `Winsock2`, `SetConsoleCtrlHandler`) that are included in the Windows SDK — no additional libraries are needed for those.

**Note:** The K8055 (Velleman) and URI USB controllers are currently Linux-only (they use libusb directly). On Windows they require their respective vendor DLLs, which are not yet integrated.

### macOS

| Dependency | Source | Notes |
|------------|--------|-------|
| Xcode Command Line Tools | `xcode-select --install` | Provides clang with C++17 support |
| libusb 1.0 | `brew install libusb` | For GMSK modem support |
| PortAudio | `brew install portaudio` | For sound card modem support |
| Mosquitto | `brew install mosquitto` | Optional; only for MQTT builds |

The `Makefile` currently targets Linux. A macOS build would need adjusted compiler flags and library paths (e.g. via `pkg-config` or Homebrew paths). The endian handling and serial port code already support macOS.

## Command-Line Usage

```
dstarrepeaterd <config-file>
```

The config file path is the only argument. All settings including log directory, verbosity levels, audio paths, and callsign lists are configured in the config file itself.

```bash
# Single instance
dstarrepeaterd /etc/dstarrepeater/dstarrepeater.ini

# Multiple instances with separate configs
dstarrepeaterd /etc/dstarrepeater/gb3in.ini
dstarrepeaterd /etc/dstarrepeater/gb7xx.ini
```

## Running as a System Service

A systemd service template is provided in the `debian/` directory. The instance name maps to the config filename:

```bash
sudo cp debian/dstarrepeaterd.dstarrepeaterd@.service /etc/systemd/system/dstarrepeaterd@.service
sudo systemctl daemon-reload

# Start with /etc/dstarrepeater/gb3in.ini
sudo systemctl enable dstarrepeaterd@gb3in
sudo systemctl start dstarrepeaterd@gb3in
```

The `%i` specifier expands to the instance name, so `dstarrepeaterd@gb3in` runs `dstarrepeaterd /etc/dstarrepeater/gb3in.ini`.

## MQTT Telemetry

When built with `make MQTT=1`, the daemon can publish live telemetry to an MQTT broker:

- **Log messages** — timestamped log output
- **Repeater status** — JSON state updates (1/sec)
- **D-Star events** — start/end/lost/BER/text in Display-Driver-compatible JSON

See [MQTT.md](MQTT.md) for full details on configuration, topic structure, and JSON format.

## Documentation

- [CONFIGURATION.md](CONFIGURATION.md) — Full configuration file reference
- [MQTT.md](MQTT.md) — MQTT telemetry setup and topic reference
- [BUILD.md](BUILD.md) — Detailed build instructions
- [CHANGELOG.md](CHANGELOG.md) — Version history

## Architecture

DStarRepeater runs as a single process with multiple threads:

- **Repeater thread** — core D-Star protocol state machine (one of four variants based on operating mode: duplex/simplex, TX-only, RX-only, or TX+RX)
- **Modem thread** — hardware interface to the radio modem
- **Controller thread** — hardware I/O for PTT, heartbeat, and external control pins

The repeater thread communicates with a gateway (ircDDBGateway or DStarGateway) via UDP using the DSRP protocol.

## Licence

This software is licenced under the GPL v2. See [COPYING.txt](COPYING.txt).

## Credits

Originally written by Jonathan Naylor G4KLX. Based on the [original DStarRepeater](https://github.com/g4klx/DStarRepeater) with wxWidgets dependency removed and ported to standalone C++17.
