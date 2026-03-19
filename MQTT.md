# MQTT Support for DStarRepeater

DStarRepeater can publish log messages and repeater status to an MQTT
broker, providing live telemetry for dashboards, home-automation systems,
or any MQTT client.

**MQTT is enabled by default.** To build without MQTT support, use `make MQTT=0`.
When built without MQTT, no MQTT code is compiled in and `libmosquitto` is not required.

A full JSON schema for all MQTT messages is available in [schema.json](schema.json).

## Building

MQTT is on by default. Install `libmosquitto-dev` before building:

```bash
sudo apt-get install libmosquitto-dev
make
```

To build without MQTT:

```bash
make MQTT=0
```

All MQTT code paths are guarded by `#if defined(MQTT)` / `#endif`.

## Configuration

MQTT settings are in the `[MQTT]` section of the INI config file:

```ini
[MQTT]
Host=127.0.0.1
Port=1883
Auth=0
Username=
Password=
Keepalive=60
Name=dstar-repeater
```

| Key | Default | Description |
|-----|---------|-------------|
| `Host` | `127.0.0.1` | MQTT broker hostname or IP address. Empty to disable. |
| `Port` | `1883` | MQTT broker port. |
| `Auth` | `0` | `1` = authenticate with username/password. |
| `Username` | *(empty)* | Broker username (when Auth=1). |
| `Password` | *(empty)* | Broker password (when Auth=1). |
| `Keepalive` | `60` | Keepalive interval in seconds. |
| `Name` | `dstar-repeater` | Client name and MQTT topic prefix. |

The MQTT log level is configured in the `[Log]` section:

```ini
[Log]
MQTTLevel=2
```

If `Host` is left empty, MQTT is disabled at runtime even when compiled in.

See [CONFIGURATION.md](CONFIGURATION.md) for the full configuration reference.

## MQTT Topics

All topics are prefixed with the `Name` value. With the default `dstar-repeater`:

### `dstar-repeater/log`

Timestamped log messages, filtered by severity. The format matches the
log file output:

```
M: 2026-03-19 14:22:01: Starting D-Star Repeater - 20260319
M: 2026-03-19 14:22:01: Using Linux 6.1.21-v8+ on aarch64
M: 2026-03-19 14:22:01: Callsign set to "GB7XX  B", gateway set to "GB7XX  G"
I: 2026-03-19 14:22:01: MQTT connected to 127.0.0.1:1883 as dstar-repeater
```

Log levels (highest to lowest):

| Letter | Level | Numeric |
|--------|-------|---------|
| `F` | Fatal | 6 |
| `E` | Error | 5 |
| `W` | Warning | 4 |
| `I` | Info | 3 |
| `M` | Message | 2 |
| `D` | Debug | 1 |

Only messages at or above the configured `MQTTLevel` are published.
The default is **2** (Message level), matching MMDVMHost behaviour.

### `dstar-repeater/status`

A JSON object published once per second with the current repeater state:

**Idle (no traffic):**
```json
{
  "myCall1": "",
  "myCall2": "",
  "yourCall": "",
  "rptCall1": "",
  "rptCall2": "",
  "tx": false,
  "rxState": "listening",
  "rptState": "listening",
  "ber": 0.0,
  "text": "",
  "status1": "",
  "status2": "",
  "status3": "",
  "status4": "",
  "status5": ""
}
```

**During an RF transmission:**
```json
{
  "myCall1": "MW0MWZ  ",
  "myCall2": "    ",
  "yourCall": "CQCQCQ  ",
  "rptCall1": "GB7XX  B",
  "rptCall2": "GB7XX  G",
  "tx": true,
  "rxState": "process_data",
  "rptState": "valid",
  "ber": 1.3,
  "text": "Hello from Pi-Star",
  "status1": "",
  "status2": "",
  "status3": "",
  "status4": "",
  "status5": ""
}
```

#### Status Field Reference

| Field | Type | Description |
|-------|------|-------------|
| `myCall1` | string | Transmitting station's callsign (8 chars, space-padded) |
| `myCall2` | string | Transmitting station's short suffix (4 chars) |
| `yourCall` | string | Destination callsign (`CQCQCQ` for CQ calls) |
| `rptCall1` | string | Repeater callsign with module letter |
| `rptCall2` | string | Gateway callsign |
| `tx` | boolean | `true` when the repeater is transmitting |
| `rxState` | string | `listening`, `process_data`, or `process_slow_data` |
| `rptState` | string | See repeater states below |
| `ber` | float | Bit Error Rate as a percentage (0.0 when idle) |
| `text` | string | Slow-data text message (if any) |
| `status1`–`status5` | string | User-configured status text messages |

#### Repeater States (`rptState`)

| Value | Meaning |
|-------|---------|
| `listening` | Idle, waiting for traffic |
| `valid` | Receiving valid RF traffic |
| `valid_wait` | Valid RF ended, waiting for ack window |
| `invalid` | Receiving RF that failed validation |
| `invalid_wait` | Invalid RF ended, waiting |
| `timeout` | Transmission timed out |
| `timeout_wait` | Timeout ended, waiting |
| `network` | Receiving traffic from the gateway |
| `shutdown` | Repeater is shut down |

### `dstar-repeater/json`

Event-driven JSON messages published at D-Star state transitions, in the
format expected by [Display-Driver](https://github.com/MW0MWZ/Display-Driver).
This means DStarRepeater can drive an OLED/TFT display via MQTT with no
modifications to Display-Driver.

**RF transmission starts:**
```json
{"D-Star":{"action":"start","source_cs":"MW0MWZ  ","source_ext":"    ","destination_cs":"CQCQCQ  ","reflector":"GB7XX  G","source":"rf"}}
```

**Transmission ends normally:**
```json
{"D-Star":{"action":"end"}}
```

**Transmission lost (watchdog timeout):**
```json
{"D-Star":{"action":"lost"}}
```

**Return to idle:**
```json
{"MMDVM":{"mode":"idle"}}
```

**BER update (once per second during active RF):**
```json
{"BER":{"mode":"D-Star","value":1.3}}
```

**RSSI update (DVAP modem, once per second during active RF):**
```json
{"RSSI":{"mode":"D-Star","value":-85}}
```

#### Display-Driver Compatibility

These messages use the exact same JSON keys and structure that
Display-Driver expects. Configure Display-Driver to subscribe to
`dstar-repeater/json`.

| Top-level key | Display-Driver parser | When published |
|---------------|----------------------|----------------|
| `D-Star` | `parseDStar()` | Transmission start, end, or lost |
| `BER` | `parseBER()` | Once per second during active RF |
| `RSSI`         | `parseRSSI()`        | Once per second during active RF (DVAP only) |
| `MMDVM` | `parseMMDVM()` | On return to idle |

## Subscribing to MQTT Output

Use any MQTT client to subscribe:

```bash
# Follow all DStarRepeater topics:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/#"

# Log messages only:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/log"

# Pretty-print status with jq:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/status" | jq .
```

## Comparison with MMDVMHost

This implementation follows MMDVMHost's MQTT conventions:

- Same `CMQTTConnection` class wrapping `libmosquitto`
- Same topic-prefix convention (`{name}/topic`)
- Same INI-style `[MQTT]` configuration section
- Same log-level filtering and format
- Same PID-based client ID scheme
- Same QoS default (EXACTLY_ONCE / QoS 2)

DStarRepeater is a **publish-only** client — it does not subscribe to
any MQTT topics or accept remote commands via MQTT.

## Troubleshooting

**"Unable to start MQTT connection"** in logs:
- Check that Mosquitto (or another MQTT broker) is running
- Verify credentials if `Auth=1`
- Check firewall rules if connecting to a remote broker

**No messages appearing:**
- Confirm the binary was built with MQTT support (default: on)
- Check that `Host` is not empty in the `[MQTT]` section
- Verify the broker: `mosquitto_pub -h 127.0.0.1 -t test -m hello`

**Log messages missing but status works:**
- The `MQTTLevel` in `[Log]` may be filtering them. Default is 2 (Message).
