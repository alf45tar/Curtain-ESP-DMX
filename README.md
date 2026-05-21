# DMX Stage Curtain Controller

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
- DMX enable uses DMX channel 507 to drive a dedicated output pin that enables DMX curtain control.
- Direct mode has priority over percentage mode when both change in the same update cycle (see the Direct vs Percentage Channel Mapping section).

## Technical Reference
### Control Mapping
- Two-pin control per curtain:
  - Left: `LEFT_CURTAIN_ENABLE_PIN` + `LEFT_CURTAIN_DIRECTION_PIN`
  - Right: `RIGHT_CURTAIN_ENABLE_PIN` + `RIGHT_CURTAIN_DIRECTION_PIN`
- DMX mapping uses a center stop zone:
  - `0..84` = rewind
  - `85..170` = stop (motor disabled)
  - `171..255` = forward

### DMX Curtain Enable

- `DMX_CURTAIN_ENABLE_DMX_CHANNEL` is `507`.
- `DMX_CURTAIN_ENABLE_PIN` is `D7`.
- When the DMX value on channel 507 is `0..127`, the pin is driven `LOW` (DMX control disabled).
- When the DMX value on channel 507 is `128..255`, the pin is driven `HIGH` (DMX control enabled).
- Use this output to gate or enable DMX curtain control on external hardware (e.g., an enable input on a remote-control interface).

#### Direct vs Percentage Channel Mapping

- **Direct channels**: `LEFT_CURTAIN_DMX_CHANNEL` and `RIGHT_CURTAIN_DMX_CHANNEL` are interpreted as motion commands (rewind/stop/forward) using the `0..84 / 85..170 / 171..255` ranges.
- **Percentage channels**: `LEFT_CURTAIN_PERCENT_DMX_CHANNEL` and `RIGHT_CURTAIN_PERCENT_DMX_CHANNEL` map `0..255` to `0..100%` and set a target position estimate used by the closed-loop logic in `CurtainController`.
- **Default precedence**: the sketch currently calls `setPercentageDMXValue()` first, then `setDMXValue()` in `applyCurtainDMX()` (in [Curtain-ESP-DMX.ino](Curtain-ESP-DMX.ino)). Because of this call order, a direct channel update will switch the controller to `DIRECT_MOTION` and override a percentage-based target when both change in the same cycle.
- **Change precedence**: to make percentage mode take priority instead, swap the call order inside `applyCurtainDMX()` so `setDMXValue()` runs before `setPercentageDMXValue()`, or centralize the decision in a new mapping function.
- **Where to look in code**: see `CurtainController::setDMXValue()` and `CurtainController::setPercentageDMXValue()` in [CurtainController.h](CurtainController.h) and the caller `applyCurtainDMX()` in [Curtain-ESP-DMX.ino](Curtain-ESP-DMX.ino).

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
| 1 Relay Module         | 1        | |
| 4 Relays Module        | 1        | Interlocked wiring for forward/rewind switching |
| 5V 1A Power Supply.    | 1        | Power supply for Wemos board and relays boards |

## Wiring
| Wemos D1 Mini Pin | Function | Notes |
|-------------------|----------|-------|
| `D1` | `LEFT_CURTAIN_ENABLE_PIN` | Left curtain relay enable output |
| `D2` | `LEFT_CURTAIN_DIRECTION_PIN` | Left curtain forward/rewind select |
| `D3` | `DIRECTION_PIN` | DMX direction control |
| `D4` | `BUILTIN_LED` | Built-in led and serial DMX output |
| `D5` | `RIGHT_CURTAIN_ENABLE_PIN` | Right curtain relay enable output |
| `D6` | `RIGHT_CURTAIN_DIRECTION_PIN` | Right curtain forward/rewind select |
| `D7` | `DMX_CURTAIN_ENABLE_PIN` | Manual override enable output driven by DMX channel 507 |
| `RX` | DMX input | Serial DMX via RS485 receiver |
| `TX` | `STARTUP_MODE_PIN` | Force default setup when LOW on boot |

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
One additonal relay is used to switch between standard remote controller to DMX stage curtain controller.

```
                   FROM REMOTE CONTROLLER                                 TO CURTAIN MOTOR
        --------------------------------------------        -----------------------------------------------
           LEFT CURTAIN              RIGHT CURTAIN             LEFT CURTAIN              RIGHT CURTAIN
         OPEN  COM  CLOSE          OPEN  COM  CLOSE          OPEN  COM  CLOSE          OPEN  COM  CLOSE
           +    +    +               +    +    +               +    +    +               +    +    +  
           |    |    |               |    |    |               |    |    |               |    |    |
           |    |    |               |    |    |               |    |    |               |    |    |
           |    |    +---------------------------------------------------+               |    |    |
           |    |                    |    |    |               |    |    |               |    |    |
           +---------------------------------------------------+    |    |               |    |    |
                |                    |    |    |               |    |    |               |    |    |
                |                    |    |    +---------------------------------------------------+
                |                    |    |                    |    |    |               |    |    |
                |                    +---------------------------------------------------+    |    |
                |                         |                    |    |    |               |    |    |
                +-------------------------+                    |    |    |               |    |    |
                                          |                    |    |    |               |    |    |
                                          |                    |    |    |               |    |    |
                                          |                    |    |    |               |    |    |
                     +---------------+    |                    |    |    |               |    |    |
                     |       +--- NC |----+                    |    |    |               |    |    |
                     |        \      |                         |    |    |               |    |    |
           D7  ------| IN      + COM |------------------------------+-------------------------+    |
                     |               |                         |         |               |         |
                     |       +--- NO |-----------------+       |         |               |         |
                     +---------------+                 |       |         |               |         |
                                                       |       |         |               |         |
                                                       |       |         |               |         |
                                                       |       |         |               |         |
                                                       |       |         |               |         |
                     +---------------+                 |       |         |               |         |
                     |       +--- NC |------           |       |         |               |         |
                     |        \      |                 |       |         |               |         |
           D1  ------| IN1     + COM |-----------------+       |         |               |         |
                     |               |                 |       |         |               |         |
                     |       +--- NO |-----------+     |       |         |               |         |
                     +---------------+           |     |       |         |               |         |
                                                 |     |       |         |               |         |
                                                 |     |       |         |               |         |
                     +---------------+           |     |       |         |               |         |
                     |       +--- NC |-------------------------+         |               |         |
                     |        \      |           |     |                 |               |         |
           D2  ------| IN2     + COM |-----------+     |                 |               |         |
                     |               |                 |                 |               |         |
                     |       +--- NO |-----------------------------------+               |         |
                     +---------------+                 |                                 |         |
                                                       |                                 |         |
                     +---------------+                 |                                 |         |
                     |       +--- NC |------           |                                 |         |
                     |        \      |                 |                                 |         |
           D5  ------| IN3     + COM |-----------------+                                 |         |
                     |               |                                                   |         |
                     |       +--- NO |-------------+                                     |         |
                     +---------------+             |                                     |         |
                                                   |                                     |         |
                                                   |                                     |         |
                     +---------------+             |                                     |         |
                     |       +--- NC |---------------------------------------------------+         |
                     |        \      |             |                                               |
           D6  ------| IN4     + COM |-------------+                                               |
                     |               |                                                             |
                     |       +--- NO |-------------------------------------------------------------+
                     +---------------+



     Power Supply          Wemos D1 Mini          4 Relays Board           1 Relay Board
        5V 1A                5V    GND               VCC   GND               VCC   GND
                              +     +                 +     +                 +    +
                              |     |                 |     |                 |    |
          VCC  ---------------+-----------------------+-----------------------+    |
                                    |                       |                      |
          GND  ---------------------+-----------------------+----------------------+

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
- Verify active-high/active-low behavior for your board. If needed, edit constructor options in [Curtain-ESP-DMX.ino](Curtain-ESP-DMX.ino).

### Software Behavior
- **DMX channels:** direct control uses `LEFT_CURTAIN_DMX_CHANNEL` and `RIGHT_CURTAIN_DMX_CHANNEL` (defaults: `510` and `511`)
- **DMX channels:** percentage control uses `LEFT_CURTAIN_PERCENT_DMX_CHANNEL` and `RIGHT_CURTAIN_PERCENT_DMX_CHANNEL` (defaults: `508` and `509`)
- **DMX channels:** DMX enable uses `DMX_CURTAIN_ENABLE_DMX_CHANNEL` (default: `507`)
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
  - In direct control, the stop DMX zone sends a brief opposite-direction pulse to stop the motor

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