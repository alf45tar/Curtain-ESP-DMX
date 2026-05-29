# 🎭 DMX Stage Curtain Controller

DMX512 curtain controller for an ESP8266.
It lets you open, close, and stop two curtain motors indipendently from DMX, Art-Net and sACN.

![Assembly](images/Assembly.jpg)

## 🔑 Key Features
- Use it to control two curtains from DMX.
- It supports physical DMX, Art-Net, and sACN.
- It works with standard lighting software, hardware controllers, or legacy DMX consoles.
- Direct mode: dedicated DMX channels give immediate open/stop/close control.
- Percentage mode: set a target position (0–100%) and the controller moves the curtain there.
- Manual bypass/Enable: a separate DMX channel can enable or disable DMX control so a local/manual remote can take over.
- Priority: direct commands take precedence over percentage targets when both change in the same update cycle.

## 🔧 Supported Hardware
- Designed for [Mottura Power 571/1 motors](https://mottura.com/en/products/power/) curtain motors.
- The motor remote interface is a 3-wire dry-contact remote: `CLOSE`, `COM`, `OPEN`. Motion is activated by short circuit CLOSE or OPEN to COM. The controller emulates this by switching the `CLOSE` or `OPEN` contact to `COM` via relays.
- The curtain motor is pulse-controlled: a short pulse in the open or close direction starts motion up to next intermediate endpoint, and a pulse in the opposite direction stops it. A long pulse open/close the curtain completely regardless of presets.
- Pulse-controlled motors respond to momentary dry-contact closures. A directional pulse starts motion in that direction; applying the opposite-direction pulse stops (or reverses) motion depending on timing and wiring.
- Short pulse: used to step toward an intermediate preset.
- Long pulse: bypass presets and run until a physical end-stop or a STOP command is received.
- For percentage-mode position estimation, set `CURTAIN_TRAVEL_TIME_MS` to match full-travel runtime.
- Use interlocked relay pairs so forward and rewind contacts cannot be energized at the same time.
- Motor power must be powered independently of the ESP8266; the ESP only switches dry contacts.

## 🔢 DMX Channel Reference
| Channel | Symbol | Purpose | Values |
|--------:|--------|:--------|:-------|
| 507 | `DMX_CURTAIN_ENABLE_DMX_CHANNEL` | DMX controller enable | `0..127` = disabled, `128..255` = enabled |
| 508 | `LEFT_CURTAIN_PERCENT_DMX_CHANNEL` | Left curtain percentage target | `0..255` → `0%..100%` (0 = open, 255 = close) |
| 509 | `RIGHT_CURTAIN_PERCENT_DMX_CHANNEL` | Right curtain percentage target | `0..255` → `0%..100%` |
| 510 | `LEFT_CURTAIN_DMX_CHANNEL` | Left curtain direct (open/stop/close) | `0..84` = rewind, `85..170` = stop, `171..255` = forward |
| 511 | `RIGHT_CURTAIN_DMX_CHANNEL` | Right curtain direct (open/stop/close) | same 3-zone mapping as left |

Note: direct (open/stop/close) commands take precedence over percentage targets when both change in the same update cycle.

## ✨ Wireless Art-Net & sACN Control
The Curtain-ESP-DMX can receive DMX data **over Wi-Fi** using the **Art-Net** and **sACN (E1.31)** lighting network protocols.

This enables full wireless integration with professional lighting software compatible with Art-Net/sACN.

The ESP8266 receives Art-Net or sACN data and converts it internally into the pulse-based curtain control logic used in wired DMX mode — maintaining full backward compatibility.

## ⚙️ Configuration with ESP-DMX-Configuration App
All network and DMX parameters (Wi-Fi credentials, protocol selection, universe, etc.) can be configured using the **[ESP-DMX-Configuration](https://github.com/alf45tar/ESP-DMX-Configuration)** application.

This app communicates directly with the device over **Wi-Fi**. No source code modification or re-compilation is required — all configuration can be done through the external app with the exception of DMX channels.

## 📦 Bill of Materials
| Item                   | Quantity | Notes |
|------------------------|----------|-------|
| Wemos D1 Mini or clone | 1        | ESP8266 host board |
| Female XLR 3 pins      | 1        | DMX input |
| Male XLR 3 pins        | 1        | DMX output (optional) |
| MAX3485                | 1        | RS485 transceiver |
| [1 Relay Module 5V](https://www.wemos.cc/en/latest/d1_mini_shield/relay.html)      | 1        | Manual bypass/enable |
| [4 Relays Module 5V](http://wiki.sunfounder.cc/index.php?title=4_Channel_5V_Relay_Module)     | 1        | Interlocked wiring for forward/rewind switching |
| 5V 1A Power Supply     | 1        | Power supply for Wemos board and relays boards. Max power consumption is 2W or 400mA. |

## 📌 Pin Mapping
| Wemos D1 Mini Pin | Function | Notes |
|-------------------|----------|-------|
| `D1` | `LEFT_CURTAIN_ENABLE_PIN` | Left curtain relay enable output |
| `D2` | `LEFT_CURTAIN_DIRECTION_PIN` | Left curtain forward/rewind select |
| `D3` | `DIRECTION_PIN` | DMX direction control |
| `D4` | `BUILTIN_LED` | Built-in led and DMX output |
| `D5` | `RIGHT_CURTAIN_ENABLE_PIN` | Right curtain relay enable output |
| `D6` | `RIGHT_CURTAIN_DIRECTION_PIN` | Right curtain forward/rewind select |
| `D7` | `DMX_CURTAIN_ENABLE_PIN` | Manual override enable output driven by DMX channel 507 |
| `RX` | DMX input | Serial DMX via RS485 receiver |
| `TX` | `STARTUP_MODE_PIN` | Force default setup when LOW on boot |

# 📐 Schematic

## 🔌 DMX connector wiring
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

DMX input and output connectors, if present, can be connected in parallel (1<->1, 2<->2 and 3<->3) to continue the DMX chain.

## 🔁 Interlocked Relay Wiring

Each curtain requires one interlocked relay pair.
One additional relay is used to switch between the local remote controller and the DMX stage curtain controller.

The local controller, curtain motor, and DMX controller are wired in parallel for the three motor lines, except for the COM wire. The bypass relay switches the motor COM between the local controller COM and the DMX controller path. When that relay is not energized, the local controller is active. When it is energized, the DMX controller takes over.

For each curtain, `D1` and `D5` act as the COM transfer relays. They connect motor COM to the next pair of directional relays, which are driven by `D2` and `D6`.

```
          FROM LOCAL REMOTE CONTROLLER                             TO CURTAIN MOTOR
   ------------------------------------------        ------------------------------------------
      LEFT CURTAIN            RIGHT CURTAIN             LEFT CURTAIN           RIGHT CURTAIN
    CLOSE  COM  OPEN        CLOSE  COM  OPEN          CLOSE  COM  OPEN        CLOSE  COM  OPEN
       +    +    +             +    +    +               +    +    +             +    +    +
       |    |    |             |    |    |               |    |    |             |    |    |
       |    |    |             |    |    |               |    |    |             |    |    |
       |    |    +-------------------------------------------------+             |    |    |
       |    |                  |    |    |               |    |    |             |    |    |
       +-------------------------------------------------+    |    |             |    |    |
            |                  |    |    |               |    |    |             |    |    |
            |                  |    |    +-------------------------------------------------+
            |                  |    |                    |    |    |             |    |    |
            |                  +-------------------------------------------------+    |    |
            |                       |                    |    |    |             |    |    |
            |                       |                    |    |    |             |    |    |
            +-----------------------+                    |    |    |             |    |    |
                                    |                    |    |    |             |    |    |
                                    |                    |    |    |             |    |    |
               +---------------+    |                    |    |    |             |    |    |
               |       +--- NC |----+                    |    |    |             |    |    |
               |        \      |                         |    |    |             |    |    |
     D7  ------| IN      + COM |------------------------------+-----------------------+    |
               |               |                         |         |             |         |
   BYPASS      |       +--- NO |-----------------+       |         |             |         |
               +---------------+                 |       |         |             |         |
                                                 |       |         |             |         |
                                                 |       |         |             |         |
                                                 |       |         |             |         |
                                                 |       |         |             |         |
               +---------------+                 |       |         |             |         |
               |       +--- NC |--               |       |         |             |         |
               |        \      |                 |       |         |             |         |
     D1  ------| IN1     + COM |-----------------+       |         |             |         |
               |               |                 |       |         |             |         |
               |       +--- NO |-----------+     |       |         |             |         |
               +---------------+           |     |       |         |             |         |
LEFT CURTAIN                               |     |       |         |             |         |
               +---------------+           |     |       |         |             |         |
               |       +--- NC |-----------------------------------+             |         |
               |        \      |           |     |       |                       |         |
     D2  ------| IN2     + COM |-----------+     |       |                       |         |
               |               |                 |       |                       |         |
               |       +--- NO |-------------------------+                       |         |
               +---------------+                 |                               |         |
                                                 |                               |         |
               +---------------+                 |                               |         |
               |       +--- NC |--               |                               |         |
               |        \      |                 |                               |         |
     D5  ------| IN3     + COM |-----------------+                               |         |
               |               |                                                 |         |
               |       +--- NO |-------------+                                   |         |
               +---------------+             |                                   |         |
RIGHT CURTAIN                                |                                   |         |
               +---------------+             |                                   |         |
               |       +--- NC |-----------------------------------------------------------+
               |        \      |             |                                   |
     D6  ------| IN4     + COM |-------------+                                   |
               |               |                                                 |
               |       +--- NO |-------------------------------------------------+
               +---------------+



Power Supply          Wemos D1 Mini          4 Relays Board           1 Relay Board
   5V 1A                5V    GND               VCC   GND               VCC   GND
                         +     +                 +     +                 +    +
                         |     |                 |     |                 |    |
     VCC  ---------------+-----------------------+-----------------------+    |
                               |                       |                      |
     GND  ---------------------+-----------------------+----------------------+
```

## 🛠️ Compile Instructions
1. Download and install the Arduino IDE from https://www.arduino.cc/en/software.
2. Install the ESP8266 board package in Arduino IDE:
  - On macOS, open `Arduino IDE > Settings...`.
  - On Windows, open `File > Preferences`.
  - Add this URL to `Additional Boards Manager URLs`:
    `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
  - Open `Tools > Board > Boards Manager`, search for `esp8266`, and install `esp8266 by ESP8266 Community`.
  - Select your ESP8266 board, such as `LOLIN(WEMOS) D1 mini (clone)`.
3. Install the two required libraries used by this sketch:
  - `LXESP8266DMX` from https://github.com/claudeheintz/LXESP8266DMX
  - `LXDMXWiFi` from https://github.com/claudeheintz/LXDMXWiFi_Library
4. Install each library in Arduino IDE:
  - Download the ZIP file from each GitHub repository page.
  - In Arduino IDE, choose `Sketch > Include Library > Add .ZIP Library...`.
  - Select the `LXESP8266DMX` ZIP file, then repeat the same step for `LXDMXWiFi`.
  - Restart Arduino IDE if the libraries do not appear immediately in `Sketch > Include Library`.
5. Open `Curtain-ESP-DMX.ino` in Arduino IDE, then compile or upload the sketch.

**Firmware Update Warning:** Disconnect the Wemos D1 mini board from the circuit before updating firmware. The board is powered by USB during programming, so external 5V power must not be connected at the same time, and the RX connection to the MAX3485 can interfere with flashing.

## 📚 Technical Reference

- Control mapping:
  - Left curtain: `LEFT_CURTAIN_ENABLE_PIN` + `LEFT_CURTAIN_DIRECTION_PIN`
  - Right curtain: `RIGHT_CURTAIN_ENABLE_PIN` + `RIGHT_CURTAIN_DIRECTION_PIN`
  - Direct DMX channels use a 3-zone mapping: `0..84` = rewind, `85..170` = stop, `171..255` = forward.
  - Percentage DMX channels map `0..255` → `0..100%` for target-position control.

- Modes:
  - Direct mode: immediate open/stop/close from direct DMX channels.
  - Percentage mode: provide a target percent; controller estimates position by runtime and moves to that target.

- Enable channel:
  - `DMX_CURTAIN_ENABLE_DMX_CHANNEL` (default `507`) drives `DMX_CURTAIN_ENABLE_PIN` (default `D7`).
  - Values `0..127` disable DMX control, `128..255` enable it.

- Input sources and priority:
  - Supported inputs: physical DMX (RS485), Art-Net, sACN.
  - HTP (highest takes precedence) merging is used across sources.
  - When both a direct command and a percentage target arrive in the same update, direct commands take precedence.

- Configuration pointers:
  - Channel and pin constants are in `Curtain-ESP-DMX.ino`.
  - Position timing (`CURTAIN_TRAVEL_TIME_MS`) is used to convert motor runtime into percentage position and should match your hardware.


## 🧩 Curtain Integration
- **Dry Contact Relay Design**: The interlocked relay approach using dry contacts is motor-agnostic. It can works with minimal or no changes in different scenario not tested by me.
- The ESP8266 only switches low-power control signals; motor power is entirely isolated.

## ⚠️ Safety Notes
- Curtain motors can pinch or jam if the travel range is not calibrated correctly.
- Double-check relay wiring and interlock behavior before powering on.
- Verify that the curtain direction matches the DMX open/close mapping before using it in production.

## 📜 License
MIT License – free to use, adapt, and improve.