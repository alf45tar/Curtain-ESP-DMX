#pragma once

#include <Arduino.h>

class CurtainController {
public:
    enum Motion : uint8_t {
        STOP = 0,
        FORWARD,
        REWIND
    };

    CurtainController(uint8_t enablePin, uint8_t directionPin,
                      bool enableActiveHigh = true,
                      bool directionForwardHigh = true)
        : enablePin(enablePin), directionPin(directionPin),
          enableActiveHigh(enableActiveHigh),
          directionForwardHigh(directionForwardHigh) {}

    void begin() {
        pinMode(enablePin, OUTPUT);
        pinMode(directionPin, OUTPUT);

        // Safe startup: disable motor output.
        writeEnable(false);
        writeDirection(FORWARD);

        lastDMXMillis = millis();
        dirty = false;
        currentMotion = STOP;
    }

    // One-channel control with center stop:
    // 0..84 = rewind, 85..170 = stop, 171..255 = forward.
    void setDMXValue(byte value) {
        if (value != dmxValue) {
            dmxValue = value;
            dirty = true;
        }
        lastDMXMillis = millis();
    }

    void update() {
        if (!dirty) {
            return;
        }

        Motion target = motionFromDMX(dmxValue);
        applyMotion(target);
        currentMotion = target;
        dirty = false;
    }

    Motion getMotion() const {
        return currentMotion;
    }

private:
    uint8_t enablePin;
    uint8_t directionPin;
    bool enableActiveHigh;
    bool directionForwardHigh;

    byte dmxValue = 127;
    bool dirty = false;
    unsigned long lastDMXMillis = 0;
    Motion currentMotion = STOP;

    Motion motionFromDMX(byte value) const {
        if (value <= 84) {
            return REWIND;
        }
        if (value >= 171) {
            return FORWARD;
        }
        return STOP;
    }

    void writeEnable(bool enabled) {
        uint8_t level = enabled ? (enableActiveHigh ? HIGH : LOW)
                                : (enableActiveHigh ? LOW : HIGH);
        digitalWrite(enablePin, level);
    }

    void writeDirection(Motion motion) {
        bool forward = (motion == FORWARD);
        uint8_t level = forward ? (directionForwardHigh ? HIGH : LOW)
                                : (directionForwardHigh ? LOW : HIGH);
        digitalWrite(directionPin, level);
    }

    void applyMotion(Motion motion) {
        if (motion == STOP) {
            writeEnable(false);
            return;
        }

        // If already moving in the same direction, ensure enabled and return.
        if (currentMotion == motion && currentMotion != STOP) {
            writeEnable(true);
            return;
        }

        // If reversing direction (was moving and now request opposite),
        // disable motor, wait a short deadtime, then change direction and re-enable.
        if (currentMotion != STOP && currentMotion != motion) {
            writeEnable(false);
            delay(250); // small deadtime to allow motor to settle
            writeDirection(motion);
            writeEnable(true);
            return;
        }

        // Was stopped -> set direction then enable immediately.
        writeDirection(motion);
        writeEnable(true);
    }
};