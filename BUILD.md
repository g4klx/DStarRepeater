# Building DStarRepeater

## Linux

### Dependencies

| Package | Ubuntu / Debian | Alpine | Purpose |
|---------|-----------------|--------|---------|
| C++17 compiler | `g++` (GCC 13+) | `g++` | Compilation |
| GNU Make | `make` | `make` | Build system |
| ALSA dev | `libasound2-dev` | `alsa-lib-dev` | Sound card modem |
| libusb 1.0 dev | `libusb-1.0-0-dev` | `libusb-dev` | USB modem (GMSK) |
| Mosquitto dev | `libmosquitto-dev` | `mosquitto-dev` | MQTT telemetry (on by default) |
| Linux headers | — | `linux-headers` | Required on Alpine only |

**Raspberry Pi (additional):**

| Package | Install | Purpose |
|---------|---------|---------|
| wiringPi | `sudo apt-get install wiringpi` or [build from source](https://github.com/WiringPi/WiringPi) | GPIO controller (on by default on ARM) |

### Install (one-liner)

**Ubuntu / Debian:**
```bash
sudo apt-get install g++ make libasound2-dev libusb-1.0-0-dev libmosquitto-dev
```

**Raspberry Pi:**
```bash
sudo apt-get install g++ make libasound2-dev libusb-1.0-0-dev libmosquitto-dev wiringpi
```

**Alpine:**
```bash
apk add g++ make alsa-lib-dev libusb-dev linux-headers mosquitto-dev
```

### Build

By default, `make` produces a release build with MQTT enabled and GPIO auto-detected (on for ARM).

```bash
make                      # default: release, MQTT on, GPIO auto
make MQTT=0               # without MQTT (no libmosquitto-dev needed)
make GPIO=0               # without GPIO (no wiringPi needed)
make BUILD=debug          # debug build with symbols and assertions
```

### Install

```bash
sudo make install
```

This installs:
- `dstarrepeaterd` to `/usr/bin`
- AMBE voice beacon data to `/usr/share/dstarrepeater/`
- Example config to `/etc/dstarrepeater/dstarrepeater.ini.example`

---

## Windows

The source includes `#if defined(_WIN32)` blocks throughout. The provided `Makefile` targets Linux only — you will need a Visual Studio project or CMakeLists.txt.

### Compiler

| Option | Requirement |
|--------|-------------|
| Visual Studio 2019+ | Enable `/std:c++17` in project properties |
| MinGW-w64 (MSYS2) | `g++ -std=c++17` |

### Libraries

| Library | Source | Required? |
|---------|--------|-----------|
| Windows SDK | Included with Visual Studio | Yes — provides Winsock2, serial port, and console APIs |
| [libusb 1.0](https://libusb.info/) | Pre-built Windows binaries available | Yes — GMSK modem support |
| [PortAudio](http://www.portaudio.com/) | Build from source or use vcpkg | Yes — sound card modem (replaces ALSA) |
| [Eclipse Mosquitto](https://mosquitto.org/) | Installer or vcpkg | Optional — only for MQTT builds |

### Linker flags

```
ws2_32.lib          Winsock2 (sockets)
portaudio.lib       Sound card modem
libusb-1.0.lib      GMSK modem
mosquitto.lib       MQTT (optional)
```

### Platform notes

- Serial ports use `CreateFile` / `ReadFile` / `WriteFile` (Win32 API)
- Sockets use Winsock2 with automatic `WSAStartup` / `WSACleanup`
- Signal handling uses `SetConsoleCtrlHandler` (Ctrl+C, close events)
- Single-instance enforcement uses a named mutex (no PID file)
- K8055 (Velleman) and URI USB controllers are stub-only on Windows — they require vendor DLLs that are not yet integrated

---

## macOS

The source includes macOS support for endian handling (`OSByteOrder.h`), serial ports (POSIX `termios`, same as Linux), and PortAudio audio. The `Makefile` currently targets Linux only.

### Dependencies (Homebrew)

```bash
xcode-select --install
brew install libusb portaudio
```

For MQTT support:
```bash
brew install mosquitto
```

### Building

The Makefile needs adjusted compiler flags and Homebrew library paths for macOS. A minimal approach:

```bash
export CFLAGS="-std=c++17 -O2 -Wall -I$(brew --prefix)/include"
export LDFLAGS="-L$(brew --prefix)/lib"
# then adjust the Makefile or build manually
```

### Platform notes

- Endian conversion uses `<libkern/OSByteOrder.h>` (via `EndianCompat.h`)
- Sound card modem uses PortAudio (same as Windows)
- Serial ports use POSIX `termios` (same as Linux)
- ALSA is not available — sound card modem requires PortAudio
