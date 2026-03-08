# MQTT Support for DStarRepeater

DStarRepeater can optionally publish log messages and repeater status to
an MQTT broker, providing the same kind of live telemetry that MMDVMHost
offers. This makes it possible to monitor D-Star repeater activity from
dashboards, home-automation systems, or any MQTT client.

**MQTT support is entirely optional.** When built without the MQTT flag
the compiled binary is identical to the tree before MQTT was added — no
MQTT code is included and no additional libraries are required.

## Building

### Without MQTT (default)

```
make
```

Produces the same `dstarrepeaterd` as the original codebase. No
`libmosquitto` dependency.

### With MQTT

Install `libmosquitto-dev` first:

```
sudo apt-get install libmosquitto-dev
```

Then build with the `MQTT` flag:

```
make MQTT=1
```

This adds `-DMQTT` to the compiler flags and links against
`-lmosquitto`. All MQTT code paths are guarded by
`#if defined(MQTT)` / `#endif`, so the flag acts as a clean on/off
switch.

## Configuration

Add the following keys to your `dstarrepeater` configuration file
(the same flat key=value format used by all other settings):

```
mqttHost=127.0.0.1
mqttPort=1883
mqttAuth=0
mqttUsername=
mqttPassword=
mqttKeepalive=60
mqttName=dstar-repeater
```

| Key             | Default           | Description                                        |
|-----------------|-------------------|----------------------------------------------------|
| `mqttHost`      | `127.0.0.1`       | MQTT broker hostname or IP address                 |
| `mqttPort`      | `1883`            | MQTT broker port                                   |
| `mqttAuth`      | `0`               | Enable authentication (`0` = off, `1` = on)        |
| `mqttUsername`   | *(empty)*         | Username when `mqttAuth=1`                         |
| `mqttPassword`  | *(empty)*         | Password when `mqttAuth=1`                         |
| `mqttKeepalive` | `60`              | Keepalive interval in seconds                      |
| `mqttName`      | `dstar-repeater`  | Client name; also used as the MQTT topic prefix    |

If `mqttHost` is left empty, MQTT is disabled at runtime even when
compiled in.

## MQTT Topics

All topics are automatically prefixed with the value of `mqttName`.
For example, with the default name `dstar-repeater`:

### `dstar-repeater/log`

Timestamped log messages, filtered by severity. The format matches the
log file output:

```
M: 2025-03-08 14:22:01: Starting D-Star Repeater - 20180403
M: 2025-03-08 14:22:01: Using wxWidgets 3.0.5 on Linux 6.1.21-v8+
M: 2025-03-08 14:22:01: Callsign set to "GB7XX  B", gateway set to "GB7XX  G"
M: 2025-03-08 14:22:01: Modem type set to "MMDVM"
I: 2025-03-08 14:22:01: MQTT connected to 127.0.0.1:1883 as dstar-repeater
M: 2025-03-08 14:22:01: Starting the D-Star repeater thread
M: 2025-03-08 14:22:01: Poll text set to "linux_mmdvm-20180403"
```

Log levels (highest to lowest):

| Letter | Level         | Numeric |
|--------|---------------|---------|
| `F`    | Fatal         | 6       |
| `E`    | Error         | 5       |
| `W`    | Warning       | 4       |
| `I`    | Info          | 3       |
| `M`    | Message       | 2       |
| `D`    | Debug         | 1       |

Only messages at or above the configured threshold are published. The
default threshold is **2** (Message level), matching MMDVMHost behaviour.

### `dstar-repeater/status`

A JSON object published once per second with the current repeater state.
This is the D-Star equivalent of MMDVMHost's status output:

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

**During a network (gateway) transmission:**
```json
{
  "myCall1": "G4KLX   ",
  "myCall2": "    ",
  "yourCall": "GB7XX  B",
  "rptCall1": "GB7XX  B",
  "rptCall2": "GB7XX  G",
  "tx": true,
  "rxState": "listening",
  "rptState": "network",
  "ber": 0.0,
  "text": "",
  "status1": "",
  "status2": "",
  "status3": "",
  "status4": "",
  "status5": ""
}
```

#### Status Field Reference

| Field      | Type    | Description                                               |
|------------|---------|-----------------------------------------------------------|
| `myCall1`  | string  | Transmitting station's callsign (8 chars, space-padded)   |
| `myCall2`  | string  | Transmitting station's short suffix (4 chars)             |
| `yourCall` | string  | Destination callsign (`CQCQCQ` for CQ calls)             |
| `rptCall1` | string  | Repeater callsign with module letter                      |
| `rptCall2` | string  | Gateway callsign                                          |
| `tx`       | boolean | `true` when the repeater is transmitting                  |
| `rxState`  | string  | `listening`, `process_data`, or `process_slow_data`       |
| `rptState` | string  | See repeater states below                                 |
| `ber`      | float   | Bit Error Rate as a percentage (0.0 when idle)            |
| `text`     | string  | Slow-data text message (if any)                           |
| `status1`–`status5` | string | User-configured status text messages             |

#### Repeater States (`rptState`)

| Value          | Meaning                                              |
|----------------|------------------------------------------------------|
| `listening`    | Idle, waiting for traffic                            |
| `valid`        | Receiving valid RF traffic                           |
| `valid_wait`   | Valid RF transmission ended, waiting for ack window  |
| `invalid`      | Receiving RF traffic that failed validation          |
| `invalid_wait` | Invalid RF ended, waiting                            |
| `timeout`      | Transmission timed out                               |
| `timeout_wait` | Timeout ended, waiting                               |
| `network`      | Receiving traffic from the gateway/network           |
| `shutdown`     | Repeater is shut down                                |

### `dstar-repeater/json`

Event-driven JSON messages published at D-Star state transitions, in the
format expected by [Display-Driver](https://github.com/MW0MWZ/Display-Driver).
This means DStarRepeater can drive an OLED/TFT display via MQTT with no
modifications to Display-Driver.

Unlike the `status` topic (polled once per second), the `json` topic only
publishes when something actually changes — a transmission starts, ends,
or is lost.

**RF transmission starts:**
```json
{"D-Star":{"action":"start","source_cs":"MW0MWZ  ","source_ext":"    ","destination_cs":"CQCQCQ  ","reflector":"GB7XX  G","source":"rf"}}
```

**Network transmission starts:**
```json
{"D-Star":{"action":"start","source_cs":"G4KLX   ","source_ext":"    ","destination_cs":"GB7XX  B","reflector":"GB7XX  G","source":"net"}}
```

**Transmission ends normally:**
```json
{"D-Star":{"action":"end"}}
```

**Transmission lost (watchdog timeout):**
```json
{"D-Star":{"action":"lost"}}
```

**Return to idle (sent after every end/lost):**
```json
{"MMDVM":{"mode":"idle"}}
```

**BER update (published once per second during active RF):**
```json
{"BER":{"mode":"D-Star","value":1.3}}
```

#### Display-Driver Compatibility

These messages use the exact same JSON keys and structure that
Display-Driver expects. If your `mqttName` is set to `dstar-repeater`,
configure Display-Driver to subscribe to `dstar-repeater/json`.

Display-Driver dispatches each top-level key to a specific parser:

| Top-level key | Display-Driver parser | When published                           |
|---------------|----------------------|------------------------------------------|
| `D-Star`      | `parseDStar()`       | Transmission start, end, or lost         |
| `BER`         | `parseBER()`         | Once per second during active RF         |
| `MMDVM`       | `parseMMDVM()`       | On return to idle                        |

## Subscribing to MQTT Output

Use any MQTT client to subscribe. For example, with `mosquitto_sub`:

```bash
# Follow all DStarRepeater topics:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/#"

# Log messages only:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/log"

# Status JSON only:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/status"

# Display-Driver-compatible events:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/json"

# Pretty-print status with jq:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/status" | jq .

# Pretty-print Display-Driver events with jq:
mosquitto_sub -h 127.0.0.1 -t "dstar-repeater/json" | jq .
```

## Comparison with MMDVMHost

This implementation follows the same conventions as MMDVMHost's MQTT
support:

- Same `CMQTTConnection` class wrapping `libmosquitto`
- Same topic-prefix convention (`{name}/topic`)
- Same log-level filtering and format
- Same PID-based client ID scheme (avoids the `time_t` truncation
  issue on 32-bit ARM)
- Same configuration key names (`mqttHost`, `mqttPort`, etc.)
- Same QoS default (EXACTLY_ONCE / QoS 2)

The key difference is that DStarRepeater is a **publish-only** client —
it does not subscribe to any MQTT topics or accept remote commands via
MQTT. The configuration file format also differs: DStarRepeater uses
flat `key=value` pairs rather than MMDVMHost's INI-style `[MQTT]`
section.

## Troubleshooting

**"Unable to start MQTT connection"** in logs:
- Check that Mosquitto (or another MQTT broker) is running on the
  configured host and port
- Verify credentials if `mqttAuth=1`
- Check firewall rules if connecting to a remote broker

**No messages appearing:**
- Confirm the binary was built with `make MQTT=1`
- Check that `mqttHost` is not empty in the config file
- Verify the broker is accessible: `mosquitto_pub -h 127.0.0.1 -t test -m hello`

**Log messages missing but status works:**
- The log level threshold may be filtering them out. Lower-priority
  messages (Debug, Trace) are not published by default.
