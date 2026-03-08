# DStarRepeater

DStarRepeater is the D-Star repeater controller for homebrew repeater hardware. It links into [ircDDBGateway](https://github.com/g4klx/ircDDBGateway) or the newer [DStarGateway](https://github.com/g4klx/DStarGateway) to provide access to D-Star networking facilities such as callsign routing, reflector linking, and ircDDB.

## Supported Hardware

- DVAP
- DV-Mega (D-Star only)
- GMSK Modems
- Soundcard repeaters (including UDRC)
- MMDVM (D-Star only)
- DV-RPTR V1, V2, and V3
- Icom Terminal and Access Point modes

## Building

Builds on 32-bit and 64-bit Linux as well as on Windows using Visual Studio 2017 (x86 and x64).

### Standard Build

```
make
```

### With MQTT Support

DStarRepeater can optionally publish live telemetry to an MQTT broker, including log messages, repeater status, and Display-Driver-compatible events for driving OLED/TFT displays.

```
sudo apt-get install libmosquitto-dev
make MQTT=1
```

MQTT support is entirely opt-in. When built without the `MQTT=1` flag, the binary is identical to the tree before MQTT was added — no MQTT code is compiled in and no additional libraries are required.

See [MQTT.md](MQTT.md) for full configuration details, topic structure, JSON examples, and Display-Driver compatibility information.

## Licence

This software is licenced under the GPL v2.
