# DStarRepeater Configuration Reference

The configuration file uses INI format with `[Section]` headers, `Key=Value` pairs, and `#` comments. The format is compatible with MMDVMHost.

To get started, copy the example config and edit for your installation:
```bash
sudo cp /etc/dstarrepeater/dstarrepeater.ini.example /etc/dstarrepeater/dstarrepeater.ini
sudo nano /etc/dstarrepeater/dstarrepeater.ini
```

Then start the daemon:
```bash
dstarrepeaterd /etc/dstarrepeater/dstarrepeater.ini
```

---

## [General]

| Key | Default | Description |
|-----|---------|-------------|
| `Callsign` | `GB3IN  C` | Repeater callsign, padded to 8 characters. 7-character base + 1-character module suffix (A–E). |
| `Gateway` | *(empty)* | Gateway callsign, padded to 8 characters (e.g. `GB3IN  G`). Leave empty if not linked. |
| `Mode` | `0` | Operating mode — see [Mode Values](#mode-values) below. |
| `Ack` | `1` | Acknowledgement type — see [Ack Values](#ack-values). Forced to `0` in Gateway, TX Only, RX Only, and TX+RX modes. |
| `Restriction` | `0` | `1` = only whitelisted callsigns accepted. |
| `RPT1Validation` | `1` | `1` = only accept headers addressed to this repeater. |
| `DTMFBlanking` | `1` | `1` = mute DTMF tones from the audio stream. |
| `ErrorReply` | `1` | `1` = reply with error message for invalid commands. |

### Mode Values

| Value | Mode | Description |
|:-----:|------|-------------|
| `0` | Duplex | Full duplex repeater (simultaneous TX and RX) |
| `1` | Simplex | Simplex repeater |
| `2` | Gateway | Gateway mode (ack forced off, RPT1 validation forced on) |
| `3` | TX Only | Transmit only |
| `4` | RX Only | Receive only |
| `5` | TX and RX | Sequential transmit then receive (split-site) |

### Ack Values

| Value | Type | Description |
|:-----:|------|-------------|
| `0` | Off | No acknowledgement |
| `1` | BER | Respond with Bit Error Rate |
| `2` | Status | Respond with status message |

---

## [Log]

Logging levels match MMDVMHost: `0`=None, `1`=Debug, `2`=Message, `3`=Info, `4`=Warning, `5`=Error, `6`=Fatal. Higher values are more selective (fewer messages).

| Key | Default | Description |
|-----|---------|-------------|
| `FilePath` | `/var/log` | Directory for daily log files (`dstarrepeaterd-YYYY-MM-DD.log`). |
| `FileLevel` | `2` | Minimum level to write to log file. `0` = no file logging. |
| `DisplayLevel` | `2` | Minimum level to write to stdout. `0` = no stdout output. |
| `MQTTLevel` | `2` | Minimum level to forward to MQTT. `0` = no MQTT logging. |

---

## [Paths]

| Key | Default | Description |
|-----|---------|-------------|
| `Data` | `/usr/share/dstarrepeater` | Directory containing AMBE voice beacon files (`.ambe` and `.indx`). |
| `Audio` | `/var/log` | Directory for DVTool audio recordings and announcements. |

---

## [Network]

| Key | Default | Description |
|-----|---------|-------------|
| `GatewayAddress` | `127.0.0.1` | IP address of the gateway (ircDDBGateway or DStarGateway). |
| `GatewayPort` | `20010` | UDP port the gateway is listening on. |
| `LocalAddress` | `127.0.0.1` | Local IP address to bind. |
| `LocalPort` | `20011` | Local UDP port. |
| `Name` | *(empty)* | Network name for multi-repeater setups. |

---

## [Modem]

| Key | Default | Description |
|-----|---------|-------------|
| `Type` | `DVAP` | Hardware modem type (case-sensitive). |

Valid modem types:

| Value | Hardware |
|-------|----------|
| `DVAP` | DVAP USB Dongle |
| `DVMEGA` | DV-Mega board |
| `DV-RPTR V1` | DV-RPTR Version 1 |
| `DV-RPTR V2` | DV-RPTR Version 2 |
| `DV-RPTR V3` | DV-RPTR Version 3 |
| `GMSK Modem` | GMSK modem (Dutch*STAR) |
| `MMDVM` | Multi-Mode Digital Voice Modem |
| `Sound Card` | Sound card modem (ALSA/PortAudio) |
| `Split` | Multi-receiver split-site |
| `Icom Access Point/Terminal Mode` | Icom radio in AP/terminal mode |

Only configure the section matching your modem type.

---

## [Times]

| Key | Default | Description |
|-----|---------|-------------|
| `Timeout` | `180` | Transmission timeout in seconds. `0` to disable. |
| `AckTime` | `500` | Acknowledgement delay in milliseconds. |

---

## [Beacon]

| Key | Default | Description |
|-----|---------|-------------|
| `Time` | `600` | Beacon interval in seconds. `0` to disable. |
| `Text` | `D-Star Repeater` | Text transmitted in the beacon slow data field. |
| `Voice` | `0` | `1` = transmit a voice beacon in addition to data. |
| `Language` | `0` | Language for voice beacon — see [Language Values](#language-values). |

### Language Values

| Value | Language |
|:-----:|----------|
| `0` | English (UK) |
| `1` | Deutsch |
| `2` | Dansk |
| `3` | Francais |
| `4` | Italiano |
| `5` | Polski |
| `6` | Espanol |
| `7` | Svenska |
| `8` | Nederlands |
| `9` | English (US) |
| `10` | Norsk |

---

## [Announcement]

| Key | Default | Description |
|-----|---------|-------------|
| `Enabled` | `0` | `1` = enable announcement recording and playback. |
| `Time` | `500` | Announcement playback interval in seconds. |
| `RecordRPT1` | *(empty)* | RPT1 callsign that triggers recording. |
| `RecordRPT2` | *(empty)* | RPT2 callsign that triggers recording. |
| `DeleteRPT1` | *(empty)* | RPT1 callsign that triggers deletion. |
| `DeleteRPT2` | *(empty)* | RPT2 callsign that triggers deletion. |

---

## [Control]

DTMF remote control via D-Star headers. When enabled, specific YOURCALL values trigger actions.

| Key | Default | Description |
|-----|---------|-------------|
| `Enabled` | `0` | `1` = enable DTMF remote control. |
| `RPT1` | *(empty)* | RPT1 callsign required for control commands. |
| `RPT2` | *(empty)* | RPT2 callsign required for control commands. |
| `Shutdown` | *(empty)* | YOURCALL value to shut down the repeater. |
| `Startup` | *(empty)* | YOURCALL value to start up the repeater. |
| `Status1`–`Status5` | *(empty)* | YOURCALL values to trigger status messages 1–5. |
| `Command1`–`Command6` | *(empty)* | YOURCALL values for custom commands 1–6. |
| `Command1Line`–`Command6Line` | *(empty)* | Shell command executed when the corresponding command is triggered. |
| `Output1`–`Output4` | *(empty)* | YOURCALL values to toggle output relays 1–4. |

---

## [Controller]

External hardware controller for PTT, LEDs, and relay outputs.

| Key | Default | Description |
|-----|---------|-------------|
| `Type` | *(empty)* | Controller type — see [Controller Types](#controller-types). |
| `SerialConfig` | `1` | Pin/output configuration preset (1–5). |
| `PTTInvert` | `0` | `1` = invert PTT output (active low). |
| `ActiveHangTime` | `0` | Seconds to hold active state after last transmission. `0` to disable. |

### Controller Types

| Value | Description |
|-------|-------------|
| *(empty)* | No controller (dummy) |
| `GPIO` | Raspberry Pi GPIO pins |
| `UDRC` | UDRC board (Raspberry Pi) |
| `Velleman K8055 - 0` | Velleman K8055 USB board (address 0–3) |
| `URI USB - 1` | URI USB relay card (address 1–6) |
| `Serial - /dev/ttyUSB0` | Serial port controller |
| `Arduino - /dev/ttyUSB0` | Arduino controller via serial |

---

## [Outputs]

Default state of output relays at startup.

| Key | Default | Description |
|-----|---------|-------------|
| `Output1` | `0` | Output relay 1. `1` = on at startup. |
| `Output2` | `0` | Output relay 2. |
| `Output3` | `0` | Output relay 3. |
| `Output4` | `0` | Output relay 4. |

---

## [Frame Logging]

| Key | Default | Description |
|-----|---------|-------------|
| `Enabled` | `0` | `1` = record all D-Star frames to `.dvtool` files in the audio directory. |

---

## [Whitelist] / [Blacklist] / [Greylist]

Callsign access control lists. Each takes a single `File` key pointing to a text file with one callsign per line.

| Key | Default | Description |
|-----|---------|-------------|
| `File` | *(not set)* | Full path to the list file. Leave commented out to disable. |

Example:
```ini
[Whitelist]
File=/etc/dstarrepeater/rptr_whitelist.dat
```

---

## Modem-Specific Settings

Only configure the section matching your `[Modem] Type`.

### [DVAP]

| Key | Default | Description |
|-----|---------|-------------|
| `Port` | *(empty)* | Serial port (e.g. `/dev/ttyUSB0`). |
| `Frequency` | `145500000` | Operating frequency in Hz. |
| `Power` | `10` | Transmit power in dBm. |
| `Squelch` | `-100` | Squelch threshold in dBm. |

### [GMSK]

| Key | Default | Description |
|-----|---------|-------------|
| `InterfaceType` | `0` | `0` = libUSB, `1` = direct/serial. |
| `Address` | `768` | USB address in decimal (768 = 0x0300). |

### [DV-RPTR V1]

| Key | Default | Description |
|-----|---------|-------------|
| `Port` | *(empty)* | Serial port (e.g. `/dev/ttyUSB0`). |
| `RXInvert` | `0` | `1` = invert received signal polarity. |
| `TXInvert` | `0` | `1` = invert transmitted signal polarity. |
| `Channel` | `0` | `0` = Channel A, `1` = Channel B. |
| `ModLevel` | `20` | Modulation level (0–100%). |
| `TXDelay` | `150` | Transmit delay in milliseconds. |

### [DV-RPTR V2]

| Key | Default | Description |
|-----|---------|-------------|
| `Connection` | `0` | `0` = USB, `1` = Network. |
| `USBPort` | *(empty)* | USB serial port. |
| `Address` | `127.0.0.1` | Network address (when Connection=1). |
| `Port` | `0` | Network port (when Connection=1). |
| `TXInvert` | `0` | `1` = invert transmitted signal polarity. |
| `ModLevel` | `20` | Modulation level (0–100%). |
| `TXDelay` | `150` | Transmit delay in milliseconds. |

### [DV-RPTR V3]

Same keys as [DV-RPTR V2].

### [DVMEGA]

| Key | Default | Description |
|-----|---------|-------------|
| `Port` | *(empty)* | Serial port (e.g. `/dev/ttyAMA0`). |
| `Variant` | `0` | `0` = Modem, `1` = Radio 2m, `2` = Radio 70cm, `3` = Radio 2m/70cm. |
| `RXInvert` | `0` | `1` = invert received signal. |
| `TXInvert` | `0` | `1` = invert transmitted signal. |
| `TXDelay` | `150` | Transmit delay in milliseconds. |
| `RXFrequency` | `145500000` | Receive frequency in Hz (radio variants only). |
| `TXFrequency` | `145500000` | Transmit frequency in Hz (radio variants only). |
| `Power` | `100` | Transmit power 0–100% (radio variants only). |

### [MMDVM]

| Key | Default | Description |
|-----|---------|-------------|
| `Port` | *(empty)* | Serial port (e.g. `/dev/ttyACM0`). |
| `RXInvert` | `0` | `1` = invert received signal. |
| `TXInvert` | `0` | `1` = invert transmitted signal. |
| `PTTInvert` | `0` | `1` = invert PTT signal. |
| `TXDelay` | `50` | Transmit delay in milliseconds. |
| `RXLevel` | `100` | Receive level (0–100%). |
| `TXLevel` | `100` | Transmit level (0–100%). |

### [Sound Card]

| Key | Default | Description |
|-----|---------|-------------|
| `RXDevice` | *(empty)* | Sound card device name for receiving. |
| `TXDevice` | *(empty)* | Sound card device name for transmitting. |
| `RXInvert` | `0` | `1` = invert received signal. |
| `TXInvert` | `0` | `1` = invert transmitted signal. |
| `RXLevel` | `1.0` | Receive level multiplier (1.0 = unity gain). |
| `TXLevel` | `1.0` | Transmit level multiplier (1.0 = unity gain). |
| `TXDelay` | `150` | Transmit delay in milliseconds. |
| `TXTail` | `50` | Transmit tail in milliseconds (extra carrier after last frame). |

### [Split]

Multi-receiver split-site configuration using UDP.

| Key | Default | Description |
|-----|---------|-------------|
| `LocalAddress` | *(empty)* | Local IP address for split communication. |
| `LocalPort` | `0` | Local UDP port. |
| `Timeout` | `0` | Timeout in milliseconds. `0` to disable. |

TX/RX names are configured as indexed keys: `TXName0`–`TXName4` and `RXName0`–`RXName24`.

### [Icom]

| Key | Default | Description |
|-----|---------|-------------|
| `Port` | *(empty)* | Serial port (e.g. `/dev/ttyUSB0`). |

---

## [MQTT]

*Only used when built with `make MQTT=1`. See [MQTT.md](MQTT.md) for full details.*

| Key | Default | Description |
|-----|---------|-------------|
| `Host` | `127.0.0.1` | MQTT broker hostname or IP. Empty to disable MQTT at runtime. |
| `Port` | `1883` | Broker port. |
| `Auth` | `0` | `1` = authenticate with username/password. |
| `Username` | *(empty)* | Broker username (when Auth=1). |
| `Password` | *(empty)* | Broker password (when Auth=1). |
| `Keepalive` | `60` | Keepalive interval in seconds. |
| `Name` | `dstar-repeater` | Client name and topic prefix. |

### MQTT Topics

With `Name=dstar-repeater`:

| Topic | Content |
|-------|---------|
| `dstar-repeater/log` | Timestamped log messages (filtered by MQTTLevel). |
| `dstar-repeater/status` | JSON repeater status (1/sec). |
| `dstar-repeater/json` | Display-Driver-compatible D-Star events. |
