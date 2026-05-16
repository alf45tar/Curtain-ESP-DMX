# DMX Curtain Controller

DMX512 curtain controller for an ESP8266.
It lets you open, close, and stop two curtain motors from DMX.

![Assembly](images/Assembly.jpg)

## User Guide
- Use it to control two curtains from DMX.
- It supports physical DMX, Art-Net, and sACN.
- It works with standard lighting software, hardware controllers, or legacy DMX consoles.

## Supported Hardware
- The curtain controller is designed to pilot 2 [Mottura Power 571/1 motors](https://mottura.com/en/products/power/).
- The curtain motor is pulse-controlled: a pulse in the open or close direction starts motion, and a pulse in the opposite direction stops it.

## Quick Facts
- Two curtains are supported.
- Direct DMX control uses dedicated channels for left and right curtains.
- Percentage DMX control uses dedicated channels for curtain position.
- Direct mode has priority over percentage mode when both are present.

## Technical Reference
### Control Mapping
- Two-pin control per curtain:
  - Left: `LEFT_CURTAIN_ENABLE_PIN` + `LEFT_CURTAIN_DIRECTION_PIN`
  - Right: `RIGHT_CURTAIN_ENABLE_PIN` + `RIGHT_CURTAIN_DIRECTION_PIN`
- DMX mapping uses a center stop zone:
  - `0..84` = rewind
  - `85..170` = stop (motor disabled)
  - `171..255` = forward

### Configuration
- Set `LEFT_CURTAIN_DMX_CHANNEL` and `RIGHT_CURTAIN_DMX_CHANNEL` for direct open/close control.
- Set `LEFT_CURTAIN_PERCENT_DMX_CHANNEL` and `RIGHT_CURTAIN_PERCENT_DMX_CHANNEL` for percentage-based positioning.
- Set left/right pin defines in [Curtain-ESP-DMX.ino](Curtain-ESP-DMX.ino) to match your relay module wiring.
- Wi-Fi and network settings still come from [LXDMXWiFiConfig.cpp](LXDMXWiFiConfig.cpp).

## Bill of Materials
| Item                   | Quantity | Notes |
|------------------------|----------|-------|
| Wemos D1 Mini or clone | 1        | ESP8266 host board |
| D1 Mini ProtoBoard     | 1        | Optional mounting board |
| Female XLR 3 pins      | 1        | DMX input |
| MAX3485                | 1        | RS485 transceiver |
| Interlocked relays     | 1 set    | Forward/rewind motor switching |
| LEDs + resistors       | optional | Status indicators |

## Wiring
| Wemos D1 Mini Pin | Function | Notes |
|-------------------|----------|-------|
| `D3` | `DIRECTION_PIN` | DMX direction control |
| `D1` | `LEFT_CURTAIN_ENABLE_PIN` | Left curtain relay enable output |
| `D2` | `LEFT_CURTAIN_DIRECTION_PIN` | Left curtain forward/rewind select |
| `D5` | `RIGHT_CURTAIN_ENABLE_PIN` | Right curtain relay enable output |
| `D6` | `RIGHT_CURTAIN_DIRECTION_PIN` | Right curtain forward/rewind select |
| `RX` | DMX input | Serial DMX via RS485 receiver |
| `TX` | DMX output | Optional serial DMX output |

# Schematic

## DMX connector wiring
```
Wemos D1 Mini                                                  DMX connector

    3V3  ----------------------------------------------+
                                                       |
                                  +---------------+    |
     RX  -------------------------| R         VCC |----+
                                  |               |
                             +----| RE/         B |----------  Data - (XLR pin 2)
                             |    |    MAX3485    |
     D3  --------------------+----| DE          A |----------  Data + (XLR pin 3)
                                  |               |
     D4  -------------------------| D         GND |---+------  Ground (XLR pin 1)
                                  +---------------+   |
                                                      |
    GND  ---------------------------------------------+
```

## Interlocked Relay Wiring

Each curtain requires one interlocked relay pair.

```
LEFT CURTAIN CONTROL (example)

                     +---------------+
          3V3  ------| VCC   +--- NC |------  (not connected)
                     |        \      |
   ENABLE_PIN  ------| IN      + COM |------  COM
                     |               |
          GND  ------| GND   +--- NO |-----------------+
                     +---------------+                 |
                                                       |
                                                       |
                     +---------------+                 |
          3V3  ------| VCC   +--- NC |------  OPEN     |
                     |        \      |                 |
DIRECTION_PIN  ------| IN      + COM |-----------------+
                     |               |
          GND  ------| GND   +--- NO |------  CLOSE
                     +---------------+


STATE TABLE:
┌─────────────┬────────────────┬─────────┬────────────────┐
│ ENABLE_PIN  │ DIRECTION_PIN  │ Motion  │ Curtain State  │
├─────────────┼────────────────┼─────────┼────────────────┤
│ LOW (OFF)   │    -           │ STOP    │ AS IS          │
│ HIGH (ON)   │ LOW            │ REWIND  │ OPEN           │
│ HIGH (ON)   │ HIGH           │ FORWARD │ CLOSE          │
└─────────────┴────────────────┴─────────┴────────────────┘

RIGHT CURTAIN: Use D5 (ENABLE) + D6 (DIRECTION) same configuration
```



### Active-High vs Active-Low:
- If relays activate when GPIO is LOW, invert the logic in [CurtainController.h](CurtainController.h) constructor:
  ```cpp
  CurtainController leftCurtain(LEFT_CURTAIN_ENABLE_PIN, LEFT_CURTAIN_DIRECTION_PIN,
                                false,  // enableActiveHigh = false
                                false); // directionForwardHigh = false
  ```

### Curtain Integration
- **Dry Contact Relay Design**: The interlocked relay approach using dry contacts is motor-agnostic. It works with:
  - AC or DC motors (any voltage/frequency your relay is rated for)
  - Brushed or brushless motors
  - Single-phase or three-phase industrial motors
  - Stepper motors with external drivers
  - Any curtain mechanism with forward/reverse control
  - The ESP8266 only switches low-power control signals; motor power is entirely isolated.

- Use relay hardware that guarantees forward and rewind cannot be active at the same time.
- Each curtain needs one enable input and one direction input on your relay hardware.
- Left and right control outputs should each drive an interlocked relay pair.
- Verify active-high/active-low behavior for your board. If needed, edit constructor options in [SHELLY-ESP-DMX.ino](SHELLY-ESP-DMX.ino).

### Software Behavior
- **DMX channels:** direct control uses `LEFT_CURTAIN_DMX_CHANNEL` and `RIGHT_CURTAIN_DMX_CHANNEL` (defaults: `510` and `511`)
- **DMX channels:** percentage control uses `LEFT_CURTAIN_PERCENT_DMX_CHANNEL` and `RIGHT_CURTAIN_PERCENT_DMX_CHANNEL` (defaults: `508` and `509`)
- **Priority:** direct mode has priority over percentage mode when both are present.
- **Direct DMX mapping**
  - `0..84` = rewind
  - `85..170` = stop
  - `171..255` = forward
- **Percentage DMX mapping**
  - `0` = fully closed
  - `255` = fully open
  - The sketch estimates curtain position from elapsed motor runtime, so set `CURTAIN_TRAVEL_TIME_MS` to match your hardware.
  - Because there is no position feedback, the estimate is only as good as the last calibrated start point.
- **Input modes**
  - Art-Net or sACN over Wi-Fi (unicast or multicast)
  - Physical DMX via RS485 serial input
  - Wi-Fi and physical DMX can be active simultaneously; highest value wins (HTP merge)
- **Failsafe**
  - Curtain movement is disabled in the stop DMX zone for direct control

### Direction Mapping
The current code interprets lower DMX values as rewind and higher values as forward. If your motor direction is opposite, swap relay wiring or invert direction logic in [CurtainController.h](CurtainController.h).

### Input Source Priority

When multiple input sources provide DMX values simultaneously, the sketch uses **HTP (Highest Takes Precedence)** merging:
- If any source sends a non-zero value, the highest value is used.
- Physical XLR DMX, Art-Net, and sACN are all treated equally in the merge.

This allows you to use multiple controllers at once (e.g., a physical console + software controller) without conflicts.

## Safety Notes
- Curtain motors can pinch or jam if the travel range is not calibrated correctly.
- Double-check relay wiring and interlock behavior before powering on.
- Verify that the curtain direction matches the DMX open/close mapping before using it in production.

## 📜 License
MIT License – free to use, adapt, and improve.
