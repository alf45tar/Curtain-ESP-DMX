#pragma once

#include <Arduino.h>

class CurtainController {
public:
    // Two ways to drive the curtain:
    // - DIRECT_MOTION uses a single DMX value with center stop.
    // - PERCENTAGE_POSITION uses a time-based position estimate.
    enum Motion : uint8_t {
        STOP = 0,
        FORWARD,
        REWIND
    };

    enum ControlMode : uint8_t {
        DIRECT_MOTION = 0,
        PERCENTAGE_POSITION
    };

    // enablePin drives the motor relay, directionPin selects forward/reverse.
    // fullTravelTimeMs should match the real end-to-end travel time.
    // endpointResyncHoldMs gives the curtain extra time to settle at 0%/100%.
    CurtainController(uint8_t enablePin, uint8_t directionPin,
                      bool enableActiveHigh = true,
                      bool directionForwardHigh = true,
                      uint32_t fullTravelTimeMs = 30000UL,
                      uint32_t endpointResyncHoldMs = 2000UL)
        : enablePin(enablePin), directionPin(directionPin),
          enableActiveHigh(enableActiveHigh),
          directionForwardHigh(directionForwardHigh),
          fullTravelTimeMs(fullTravelTimeMs),
          endpointResyncHoldMs(endpointResyncHoldMs) {}

    void begin() {
        // Configure outputs and reset all runtime state.
        pinMode(enablePin, OUTPUT);
        pinMode(directionPin, OUTPUT);

        applyMotion(STOP);

        currentPositionPercent = 0;
        targetPositionPercent = 0;
        motionStartPercent = 0;
        motionStartMillis = millis();
        dirtyDirect = false;
        dirtyPercent = false;
        currentMotion = STOP;
        controlMode = DIRECT_MOTION;
    }

    // One-channel control with center stop:
    // 0..84 = rewind, 85..170 = stop, 171..255 = forward.
    void setDMXValue(byte value) {
        // Direct mode only cares about the latest DMX value.
        if (value != dmxValue) {
            controlMode = DIRECT_MOTION;
            dmxValue = value;
            dirtyDirect = true;
        }
        lastDMXMillis = millis();
    }

    // Percentage control:
    // 0 = fully closed, 100 = fully open.
    // The class estimates position from run time, so it needs a calibrated
    // full travel time to make percentage moves accurate.
    void setPercentageDMXValue(byte value) {
        // Percentage mode updates the target position, not the motor directly.
        uint8_t nextPositionPercent = dmxValueToPercent(value);
        if (nextPositionPercent != targetPositionPercent) {
            controlMode = PERCENTAGE_POSITION;
            targetPositionPercent = nextPositionPercent;
            dirtyPercent = true;
        }
        lastDMXMillis = millis();
    }

    void update() {
        // Keep the state machine simple: the selected mode controls the update path.
        servicePulseSequence();
        refreshPositionEstimate();

        if (pulsePhase != PULSE_NONE) {
            return;
        }

        switch (controlMode) {
            case DIRECT_MOTION:
                updateDirectMode();
                break;
            case PERCENTAGE_POSITION:
                updatePercentageMode();
                break;
        }
    }

    Motion getMotion() const {
        return currentMotion;
    }

    bool isDirectDirty() const {
        return dirtyDirect;
    }

    bool isPercentDirty() const {
        return dirtyPercent;
    }

    void clearDirectDirty() {
        dirtyDirect = false;
    }

    void clearPercentDirty() {
        dirtyPercent = false;
    }

private:
    // Hardware wiring and motion calibration.
    uint8_t enablePin;
    uint8_t directionPin;
    bool enableActiveHigh;
    bool directionForwardHigh;
    uint32_t fullTravelTimeMs;

    // Last received DMX values and the estimated curtain state.
    byte dmxValue = 127;
    uint8_t targetPositionPercent = 0;
    uint8_t currentPositionPercent = 0;
    uint8_t motionStartPercent = 0;
    bool dirtyDirect = false;      // Set when direct DMX value changes
    bool dirtyPercent = false;     // Set when percentage DMX value changes
    unsigned long lastDMXMillis = 0;
    unsigned long motionStartMillis = 0;
    ControlMode controlMode = DIRECT_MOTION;
    Motion currentMotion = STOP;
    // Additional time to stay active after the fader reaches an endpoint.
    uint32_t endpointResyncHoldMs;
    uint32_t pulseDurationMs = 500UL;
    uint32_t reverseGapMs = 500UL;
    unsigned long pulseStartMillis = 0;
    unsigned long reverseGapStartMillis = 0;
    Motion pulseMotion = STOP;
    Motion pendingMotion = STOP;

    enum PulsePhase : uint8_t {
        PULSE_NONE = 0,
        PULSE_START,
        PULSE_STOP,
        PULSE_REVERSE_GAP
    };

    PulsePhase pulsePhase = PULSE_NONE;

    Motion motionFromDMX(byte value) const {
        // The middle third is stop; the low and high ends map to motion.
        if (value <= 84) {
            return REWIND;
        }
        if (value >= 171) {
            return FORWARD;
        }
        return STOP;
    }

    uint8_t dmxValueToPercent(byte value) const {
        // Convert the 0-255 DMX value into a 0-100 target position.
        return (uint32_t)value * 100UL / 255UL;
    }

    void refreshPositionEstimate() {
        // Estimate position from elapsed runtime while the motor is moving.
        if (currentMotion == STOP || fullTravelTimeMs == 0) {
            return;
        }

        unsigned long elapsed = millis() - motionStartMillis;
        if (elapsed >= fullTravelTimeMs) {
            currentPositionPercent = (currentMotion == REWIND) ? 100 : 0;
            return;
        }

        uint8_t traveledPercent = (uint32_t)elapsed * 100UL / fullTravelTimeMs;
        if (currentMotion == REWIND) {
            uint16_t nextPosition = motionStartPercent + traveledPercent;
            currentPositionPercent = nextPosition > 100 ? 100 : (uint8_t)nextPosition;
        } else {
            currentPositionPercent = (traveledPercent >= motionStartPercent)
                ? 0
                : (uint8_t)(motionStartPercent - traveledPercent);
        }
    }

    void startMotion(Motion motion) {
        // Capture the current estimated position before changing direction.
        applyMotion(motion);
        if (motion != STOP) {
            motionStartPercent = currentPositionPercent;
            motionStartMillis = millis();
        }
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

    void startEnablePulse(Motion motion) {
        writeDirection(motion);
        writeEnable(true);
        pulsePhase = PULSE_START;
        pulseMotion = motion;
        pulseStartMillis = millis();
        currentMotion = motion;
    }

    void startStopPulse(Motion motion) {
        writeDirection(oppositeMotion(motion));
        writeEnable(true);
        pulsePhase = PULSE_STOP;
        pulseMotion = motion;
        pulseStartMillis = millis();
    }

    void startReverseGap(Motion motion) {
        pendingMotion = motion;
        reverseGapStartMillis = millis();
        pulsePhase = PULSE_REVERSE_GAP;
    }

    Motion oppositeMotion(Motion motion) const {
        return (motion == FORWARD) ? REWIND : FORWARD;
    }

    void applyMotion(Motion motion) {
        if (motion == STOP) {
            if (currentMotion != STOP && pulsePhase == PULSE_NONE) {
                // Stop by setting the opposite direction, then pulsing enable.
                startStopPulse(currentMotion);
            }
            digitalWrite(directionPin, LOW);
            return;
        }

        // If already moving in the same direction, no new pulse is needed.
        if (currentMotion == motion && pulsePhase == PULSE_NONE) {
            return;
        }

        // If a pulse sequence is already running, let update() finish it.
        if (pulsePhase != PULSE_NONE) {
            pendingMotion = motion;
            return;
        }

        // Was stopped -> set direction first, then pulse enable to start moving.
        if (currentMotion == STOP) {
            startEnablePulse(motion);
            return;
        }

        // If reversing direction, stop first and let the queued motion start after the gap.
        if (currentMotion != motion) {
            startReverseGap(motion);
            startStopPulse(currentMotion);
        }
    }

    void servicePulseSequence() {
        unsigned long now = millis();

        if (pulsePhase == PULSE_NONE) {
            return;
        }

        if ((pulsePhase == PULSE_START || pulsePhase == PULSE_STOP) &&
            (now - pulseStartMillis >= pulseDurationMs)) {
            writeEnable(false);
            digitalWrite(directionPin, LOW);

            if (pulsePhase == PULSE_STOP) {
                currentMotion = STOP;
                if (pendingMotion != STOP && pendingMotion != pulseMotion) {
                    startReverseGap(pendingMotion);
                } else {
                    pendingMotion = STOP;
                    pulsePhase = PULSE_NONE;
                }
            } else {
                pulsePhase = PULSE_NONE;
            }
            return;
        }

        if (pulsePhase == PULSE_REVERSE_GAP &&
            (now - reverseGapStartMillis >= reverseGapMs)) {
            Motion motion = pendingMotion;
            pendingMotion = STOP;
            pulsePhase = PULSE_NONE;
            startEnablePulse(motion);
        }
    }

    void updateDirectMode() {
        // Direct mode reacts only when the DMX value changes.
        refreshPositionEstimate();

        if (!dirtyDirect) {
            return;
        }

        Motion target = motionFromDMX(dmxValue);
        if (target == currentMotion) {
            applyMotion(target);
        } else {
            startMotion(target);
        }
        dirtyDirect = false;
        dirtyPercent = false;
    }

    void updatePercentageMode() {
        // Percentage mode keeps adjusting toward the latest target position.
        refreshPositionEstimate();

        unsigned long elapsed = millis() - motionStartMillis;
        bool targetIsEndpoint = (targetPositionPercent == 0) || (targetPositionPercent == 100);
        dirtyPercent = false;

        bool reachedTarget = false;
        if (currentMotion == REWIND) {
            reachedTarget = (currentPositionPercent >= targetPositionPercent);
        } else if (currentMotion == FORWARD) {
            reachedTarget = (currentPositionPercent <= targetPositionPercent);
        }

        if (reachedTarget) {
            if (currentMotion != STOP) {
                // At the end stops, hold the motor on a little longer so the
                // estimate can catch up with the real curtain position.
                if (targetIsEndpoint && elapsed < (fullTravelTimeMs + endpointResyncHoldMs)) {
                    return;
                }

                currentPositionPercent = targetPositionPercent;
                // Don't stop at endpoints - let hardware limit stop the motor naturally
                if (!targetIsEndpoint) {
                    applyMotion(STOP);
                    currentMotion = STOP;
                }
            }
            return;
        }

        Motion desiredMotion = (targetPositionPercent > currentPositionPercent) ? REWIND : FORWARD;
        if (currentMotion == STOP) {
            startMotion(desiredMotion);
            return;
        }

        if (currentMotion != desiredMotion) {
            startMotion(desiredMotion);
        }
    }
};