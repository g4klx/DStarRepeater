# DStarRepeater Configuration Reference

> **Note:** The config parser treats any line beginning with `#` as a
> comment, and blank lines are skipped. Use `dstarrepeater.example` as
> your starting point for a clean config.

**Config file location:** `/etc/dstarrepeater`
(or `/etc/dstarrepeater_<name>` when using the `-name` command line option)

**Format:** `key=value` — one setting per line, no spaces around the `=`.
Boolean values use `0` for off/false and `1` for on/true.

---

## Callsign Settings

| Setting | Description |
|---------|-------------|
| `callsign=GB3IN  C` | Repeater callsign, padded to 8 characters. 7-character base callsign + 1-character module suffix (A, B, C, D, or E). |
| `gateway=` | Gateway callsign, padded to 8 characters (e.g. `GB3IN  G`). Leave empty if not linked to a gateway. |
| `mode=0` | Operating mode — see [Mode Values](#mode-values) below. |
| `ack=1` | Acknowledgement type — see [Ack Values](#ack-values) below. Forced to `0` in Gateway, TX Only, RX Only, and TX and RX modes. |
| `restriction=0` | `1` = only whitelisted callsigns accepted. |
| `rpt1Validation=1` | `1` = only accept headers addressed to this repeater. Forced to `1` in Gateway mode. |
| `dtmfBlanking=1` | `1` = mute DTMF tones from the audio stream. |
| `errorReply=1` | `1` = reply with an error message for invalid commands. |

### Mode Values

| Value | Mode | Description |
|:-----:|------|-------------|
| `0` | Duplex | Full duplex repeater (simultaneous TX and RX) |
| `1` | Simplex | Simplex repeater |
| `2` | Gateway | Gateway mode (ack forced off, RPT1 validation forced on) |
| `3` | TX Only | Transmit only (ack forced off) |
| `4` | RX Only | Receive only (ack forced off) |
| `5` | TX and RX | Sequential transmit then receive |

### Ack Values

| Value | Type | Description |
|:-----:|------|-------------|
| `0` | Off | No acknowledgement |
| `1` | BER | Respond with Bit Error Rate |
| `2` | Status | Respond with status message |

---

## Network Settings

| Setting | Description |
|---------|-------------|
| `gatewayAddress=127.0.0.1` | IP address of the [ircDDBGateway](https://github.com/g4klx/ircDDBGateway) or [DStarGateway](https://github.com/g4klx/DStarGateway). |
| `gatewayPort=20010` | UDP port the gateway is listening on. |
| `localAddress=127.0.0.1` | Local IP address to bind for gateway communication. |
| `localPort=20011` | Local UDP port for gateway communication. |
| `networkName=` | Network name identifier for multi-repeater setups. Leave empty for a single repeater. |

---

## Modem Type

| Setting | Description |
|---------|-------------|
| `modemType=DVAP` | The type of modem hardware connected to this repeater. |

Must be one of the following strings (case-sensitive):

- `DVAP`
- `DVMEGA`
- `DV-RPTR V1`
- `DV-RPTR V2`
- `DV-RPTR V3`
- `GMSK Modem`
- `MMDVM`
- `Sound Card`
- `Split`
- `Icom Access Point/Terminal Mode`

Only the settings for your selected modem type need to be configured — see [Modem-Specific Settings](#modem-specific-settings).

---

## Timing

| Setting | Description |
|---------|-------------|
| `timeout=180` | Transmission timeout in seconds. `0` to disable. |
| `ackTime=500` | Acknowledgement delay in milliseconds. |

---

## Beacon

| Setting | Description |
|---------|-------------|
| `beaconTime=600` | Beacon interval in seconds. `0` to disable. |
| `beaconText=D-Star Repeater` | Text transmitted in the beacon slow data field. |
| `beaconVoice=0` | `1` = transmit a voice beacon in addition to data. |
| `language=0` | Language for voice beacon text-to-speech — see [Language Values](#language-values) below. |

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

## Announcement

| Setting | Description |
|---------|-------------|
| `announcementEnabled=0` | `1` = enable the announcement/recording feature. |
| `announcementTime=500` | Announcement playback interval in milliseconds. |
| `announcementRecordRPT1=` | RPT1 callsign that triggers recording. |
| `announcementRecordRPT2=` | RPT2 callsign that triggers recording. |
| `announcementDeleteRPT1=` | RPT1 callsign that triggers deletion. |
| `announcementDeleteRPT2=` | RPT2 callsign that triggers deletion. |

---

## DTMF Control

| Setting | Description |
|---------|-------------|
| `controlEnabled=0` | `1` = enable DTMF remote control. |
| `controlRPT1=` | RPT1 callsign required for control commands. |
| `controlRPT2=` | RPT2 callsign required for control commands. |
| `controlShutdown=` | DTMF sequence to remotely shut down the repeater. |
| `controlStartup=` | DTMF sequence to remotely start up the repeater. |
| `controlStatus1=` | DTMF sequence to trigger status message 1. |
| `controlStatus2=` | DTMF sequence to trigger status message 2. |
| `controlStatus3=` | DTMF sequence to trigger status message 3. |
| `controlStatus4=` | DTMF sequence to trigger status message 4. |
| `controlStatus5=` | DTMF sequence to trigger status message 5. |
| `controlCommand1=` | DTMF sequence for custom command 1. |
| `controlCommand1Line=` | Shell command executed when command 1 is received. |
| `controlCommand2=` | DTMF sequence for custom command 2. |
| `controlCommand2Line=` | Shell command executed when command 2 is received. |
| `controlCommand3=` | DTMF sequence for custom command 3. |
| `controlCommand3Line=` | Shell command executed when command 3 is received. |
| `controlCommand4=` | DTMF sequence for custom command 4. |
| `controlCommand4Line=` | Shell command executed when command 4 is received. |
| `controlCommand5=` | DTMF sequence for custom command 5. |
| `controlCommand5Line=` | Shell command executed when command 5 is received. |
| `controlCommand6=` | DTMF sequence for custom command 6. |
| `controlCommand6Line=` | Shell command executed when command 6 is received. |
| `controlOutput1=` | DTMF sequence to toggle output relay 1. |
| `controlOutput2=` | DTMF sequence to toggle output relay 2. |
| `controlOutput3=` | DTMF sequence to toggle output relay 3. |
| `controlOutput4=` | DTMF sequence to toggle output relay 4. |

---

## External Controller

| Setting | Description |
|---------|-------------|
| `controllerType=` | Hardware controller type for PTT, LEDs, and relays — see [Controller Types](#controller-types) below. Leave empty for none. |
| `serialConfig=1` | Hardware config selection (1–5). Selects pin/output mappings on the controller. |
| `pttInvert=0` | `1` = invert PTT output (active low). |
| `activeHangTime=0` | Seconds to hold active state after last transmission ends. `0` to disable. |

### Controller Types

| Value | Description |
|-------|-------------|
| *(empty)* | No controller |
| `GPIO` | Raspberry Pi GPIO pins |
| `UDRC` | UDRC board (Raspberry Pi) |
| `Velleman K8055 - 0` | Velleman K8055 USB board at address 0–3 |
| `URI USB - 1` | URI USB relay card at address 1–6 |
| `Serial - /dev/ttyUSB0` | Serial port controller |
| `Arduino - /dev/ttyUSB0` | Arduino controller via serial |

---

## Output Relay Defaults

| Setting | Description |
|---------|-------------|
| `output1=0` | Default state of output relay 1 at startup. `1` = on. |
| `output2=0` | Default state of output relay 2 at startup. `1` = on. |
| `output3=0` | Default state of output relay 3 at startup. `1` = on. |
| `output4=0` | Default state of output relay 4 at startup. `1` = on. |

---

## Logging

| Setting | Description |
|---------|-------------|
| `logging=0` | `1` = enable logging to file. |

---

## Window Position (GUI only)

| Setting | Description |
|---------|-------------|
| `windowX=-1` | Window X position. `-1` for system default. |
| `windowY=-1` | Window Y position. `-1` for system default. |

---

## Modem-Specific Settings

Only configure the section that matches your `modemType`.

### DVAP

| Setting | Description |
|---------|-------------|
| `dvapPort=` | Serial port (e.g. `/dev/ttyUSB0`). |
| `dvapFrequency=145500000` | Operating frequency in Hz (e.g. `438500000` = 438.500 MHz). |
| `dvapPower=10` | Transmit power in dBm. |
| `dvapSquelch=-100` | Squelch level in dBm. Signals below this are ignored. |

### GMSK Modem

| Setting | Description |
|---------|-------------|
| `gmskAddress=768` | USB address of the GMSK modem (decimal). Default 768 = 0x0300 hex. |

### DV-RPTR V1

| Setting | Description |
|---------|-------------|
| `dvrptr1Port=` | Serial port (e.g. `/dev/ttyUSB0`). |
| `dvrptr1RXInvert=0` | `1` = invert received signal polarity. |
| `dvrptr1TXInvert=0` | `1` = invert transmitted signal polarity. |
| `dvrptr1Channel=0` | `0` = Channel A, `1` = Channel B. |
| `dvrptr1ModLevel=20` | Modulation level (0–100%). |
| `dvrptr1TXDelay=150` | Transmit delay in milliseconds. |

### DV-RPTR V2

| Setting | Description |
|---------|-------------|
| `dvrptr2Connection=0` | `0` = USB, `1` = Network. |
| `dvrptr2USBPort=` | USB serial port (when connection = `0`). |
| `dvrptr2Address=127.0.0.1` | Network address (when connection = `1`). |
| `dvrptr2Port=0` | Network port (when connection = `1`). |
| `dvrptr2TXInvert=0` | `1` = invert transmitted signal polarity. |
| `dvrptr2ModLevel=20` | Modulation level (0–100%). |
| `dvrptr2TXDelay=150` | Transmit delay in milliseconds. |

### DV-RPTR V3

| Setting | Description |
|---------|-------------|
| `dvrptr3Connection=0` | `0` = USB, `1` = Network. |
| `dvrptr3USBPort=` | USB serial port (when connection = `0`). |
| `dvrptr3Address=127.0.0.1` | Network address (when connection = `1`). |
| `dvrptr3Port=0` | Network port (when connection = `1`). |
| `dvrptr3TXInvert=0` | `1` = invert transmitted signal polarity. |
| `dvrptr3ModLevel=20` | Modulation level (0–100%). |
| `dvrptr3TXDelay=150` | Transmit delay in milliseconds. |

### DVMEGA

| Setting | Description |
|---------|-------------|
| `dvmegaPort=` | Serial port (e.g. `/dev/ttyAMA0`). |
| `dvmegaVariant=0` | `0` = Modem, `1` = Radio 2m, `2` = Radio 70cm, `3` = Radio 2m/70cm. |
| `dvmegaRXInvert=0` | `1` = invert received signal polarity. |
| `dvmegaTXInvert=0` | `1` = invert transmitted signal polarity. |
| `dvmegaTXDelay=150` | Transmit delay in milliseconds. |
| `dvmegaRXFrequency=145500000` | Receive frequency in Hz (radio variants only). |
| `dvmegaTXFrequency=145500000` | Transmit frequency in Hz (radio variants only). |
| `dvmegaPower=100` | Transmit power as a percentage (0–100%, radio variants only). |

### MMDVM

| Setting | Description |
|---------|-------------|
| `mmdvmPort=` | Serial port (e.g. `/dev/ttyACM0`). |
| `mmdvmRXInvert=0` | `1` = invert received signal polarity. |
| `mmdvmTXInvert=0` | `1` = invert transmitted signal polarity. |
| `mmdvmPTTInvert=0` | `1` = invert PTT signal. |
| `mmdvmTXDelay=50` | Transmit delay in milliseconds. |
| `mmdvmRXLevel=100` | Receive audio level (0–100%). |
| `mmdvmTXLevel=100` | Transmit audio level (0–100%). |

### Sound Card

| Setting | Description |
|---------|-------------|
| `soundCardRXDevice=` | Sound card device name for receiving audio. |
| `soundCardTXDevice=` | Sound card device name for transmitting audio. |
| `soundCardRXInvert=0` | `1` = invert received signal polarity. |
| `soundCardTXInvert=0` | `1` = invert transmitted signal polarity. |
| `soundCardRXLevel=1.0000` | Receive level multiplier (1.0 = unity gain). |
| `soundCardTXLevel=1.0000` | Transmit level multiplier (1.0 = unity gain). |
| `soundCardTXDelay=150` | Transmit delay in milliseconds. |
| `soundCardTXTail=50` | Transmit tail in milliseconds (extra carrier after last frame). |

### Icom Access Point/Terminal Mode

| Setting | Description |
|---------|-------------|
| `icomPort=` | Serial port (e.g. `/dev/ttyUSB0`). |

### Split

| Setting | Description |
|---------|-------------|
| `splitLocalAddress=` | Local IP address for split RX/TX communication. |
| `splitLocalPort=0` | Local UDP port for split communication. |
| `splitTimeout=0` | Split timeout in seconds. `0` to disable. |

Split TX/RX names are stored as indexed entries (`splitTXName0`, `splitRXName0`, etc.) — up to 5 transmitters and 25 receivers.

---

## MQTT Settings

*Only used when built with `make MQTT=1`. See [MQTT.md](MQTT.md) for full details on topics, JSON format, and Display-Driver compatibility.*

| Setting | Description |
|---------|-------------|
| `mqttHost=127.0.0.1` | MQTT broker hostname or IP address. Set to empty to disable MQTT at runtime even when compiled in. |
| `mqttPort=1883` | MQTT broker port. |
| `mqttAuth=0` | `1` = authenticate with the broker using `mqttUsername` and `mqttPassword`. |
| `mqttUsername=` | Broker username (when `mqttAuth=1`). |
| `mqttPassword=` | Broker password (when `mqttAuth=1`). |
| `mqttKeepalive=60` | Keepalive interval in seconds. Minimum 5. |
| `mqttName=dstar-repeater` | Client name and topic prefix for all published messages. |

### MQTT Topics

With the default `mqttName` of `dstar-repeater`, the following topics are published:

| Topic | Content |
|-------|---------|
| `dstar-repeater/log` | Timestamped log messages, filtered by severity. |
| `dstar-repeater/status` | JSON repeater status, published once per second. |
| `dstar-repeater/json` | Display-Driver-compatible events at state transitions. |
