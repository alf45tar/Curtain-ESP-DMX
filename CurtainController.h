#ifndef CURTAIN_CONTROLLER_H
#define CURTAIN_CONTROLLER_H

#include <Arduino.h>

class CurtainController {
public:
    enum Motion : uint8_t {
        STOP = 0,
        FORWARD, // Opens the curtain
        REWIND   // Closes the curtain
    };

    enum ControlMode : uint8_t {
        DIRECT_MOTION = 0,
        PERCENTAGE_POSITION
    };

    // Constructor
    CurtainController(uint8_t openPin, uint8_t closePin,
                      bool relaysActiveHigh = false,
                      uint32_t fullTravelTimeMs = 20000UL,
                      uint32_t stopPulseMs = 100UL)
        : _openPin(openPin),
          _closePin(closePin),
          _relaysActiveHigh(relaysActiveHigh),
          _fullTravelTimeMs(fullTravelTimeMs),
          _stopPulseMs(stopPulseMs),
          _currentMode(DIRECT_MOTION),
          _currentMotion(STOP),
          _targetMotion(STOP),
          _pulseState(IDLE),
          _calculatedPosition(0.0f),
          _currentPosition(0),
          _targetPosition(0),
          _isSettling(false),
          _motorStartTimestampMs(0),
          _motorStartPosPercentage(0.0f),
          _stateTimerMs(0),
          _lastRawDMXDirect(255),  // Initialized out-of-band to force first-run processing
          _lastRawDMXPercent(255) // Initialized out-of-band to force first-run processing
          {}

    // Core Methods
    void begin() {
        pinMode(_openPin, OUTPUT);
        pinMode(_closePin, OUTPUT);
        
        // Safely pull relays to their logical offline/inactive state
        writeHardwarePins(STOP, false);
    }

    void setDMXValue(byte value) {
        // Exit early if value has not modified since the previous cycle
        if (value == _lastRawDMXDirect) return;
        
        _lastRawDMXDirect = value;
        _currentMode = DIRECT_MOTION;
        _isSettling = false; // Direct manual override breaks out of positional settle locks
        
        Motion manualCommand = STOP;
        if (value <= 84) {
            manualCommand = REWIND;
        } else if (value >= 171) {
            manualCommand = FORWARD;
        } else {
            manualCommand = STOP;
        }
        
        initiateTransition(manualCommand);
    }

    void setPercentageDMXValue(byte value) {
        // Exit early if value has not modified since the previous cycle
        if (value == _lastRawDMXPercent) return;
        
        _lastRawDMXPercent = value;
        _currentMode = PERCENTAGE_POSITION;
        
        // Ignore updates if system is waiting out its physical braking/settle cycle
        if (_isSettling) return;

        // Map 0...255 dynamically to 0...100%
        uint8_t newTarget = (uint16_t)(value * 100) / 255;
        
        // Hysteresis Filter Evaluation while at rest
        if (_currentMotion == STOP) {
            int16_t variance = (int16_t)newTarget - (int16_t)_currentPosition;
            if (abs(variance) <= 3) { // 3% static dead-band
                return; 
            }
        }
        
        if (_targetPosition != newTarget) {
            _targetPosition = newTarget;
        }
    }

    void update() {
        // 1. Process physical time movement tracking
        updatePositionTracking();
        
        // 2. Handle closed-loop tracking targets in percentage mode
        if (_currentMode == PERCENTAGE_POSITION && !_isSettling) {
            
            if (_currentPosition == _targetPosition) {
                if (_currentMotion == STOP) {
                    _targetPosition = _currentPosition;
                } else {
                    initiateTransition(STOP);
                    _isSettling = true; // Settle lock engaged until the stop pulse completes
                }
            } 
            // Initiate motion toward target if not yet met
            else {
                Motion requiredMotion = (_targetPosition > _currentPosition) ? FORWARD : REWIND;
                initiateTransition(requiredMotion);
            }
        }
        
        // 3. Tick Pulse State Machine kernel
        processPulseStateMachine();
    }
    
    // Getters
    uint8_t getCurrentPosition() const { return _currentPosition; }
    Motion getMotion() const { return _currentMotion; }

private:
    // Internal Hardware Pulse States
    enum PulseState : uint8_t {
        IDLE,
        RUNNING,
        STOP_PULSE
    };

    // Hardware Pins
    uint8_t _openPin;
    uint8_t _closePin;
    
    // Logic Configuration Flags
    bool _relaysActiveHigh;

    // Timing Configuration Constraints
    uint32_t _fullTravelTimeMs;
    uint32_t _stopPulseMs;

    // Runtime State Variables
    ControlMode _currentMode;
    Motion _currentMotion;
    Motion _targetMotion;      
    PulseState _pulseState;
    
    float _calculatedPosition; 
    uint8_t _currentPosition;  
    uint8_t _targetPosition;   
    
    bool _isSettling;          
    
    // Total Accumulation Safe Precision Markers
    uint32_t _motorStartTimestampMs;
    float _motorStartPosPercentage;
    
    // Time Tracking Stamps
    uint32_t _stateTimerMs;

    // Change Detection History Trackers
    byte _lastRawDMXDirect;
    byte _lastRawDMXPercent;

    // Private Helper Functions
    void writeRelayPin(uint8_t pin, bool active) {
        bool pinValue = _relaysActiveHigh ? active : !active;
        digitalWrite(pin, pinValue ? HIGH : LOW);
    }

    void writeHardwarePins(Motion motionState, bool stopPulseActive) {
        bool openRelayActive = false;
        bool closeRelayActive = false;

        if (stopPulseActive) {
            openRelayActive = true;
            closeRelayActive = true;
        } else if (motionState == FORWARD) {
            openRelayActive = true;
        } else if (motionState == REWIND) {
            closeRelayActive = true;
        }

        writeRelayPin(_openPin, openRelayActive);
        writeRelayPin(_closePin, closeRelayActive);
    }

    void initiateTransition(Motion nextMotion) {
        if (_targetMotion == nextMotion) return;
        
        _targetMotion = nextMotion;
        
        if (_pulseState == IDLE) {
            if (_targetMotion == STOP) {
                _currentMotion = STOP;
                writeHardwarePins(STOP, false);
                return;
            }

            _currentMotion = _targetMotion;
            _pulseState = RUNNING;
            _stateTimerMs = millis();
            
            // Log reference point before the motor begins moving.
            _motorStartTimestampMs = _stateTimerMs; 
            _motorStartPosPercentage = _calculatedPosition;
            writeHardwarePins(_currentMotion, false);
        } else if (_pulseState == RUNNING && _targetMotion != _currentMotion) {
            _pulseState = STOP_PULSE;
            _stateTimerMs = millis();
        }
    }

    void updatePositionTracking() {
        if (_currentMotion != STOP && _pulseState != IDLE) {
            uint32_t currentRunDuration = millis() - _motorStartTimestampMs;
            float projectedDelta = ((float)currentRunDuration / (float)_fullTravelTimeMs) * 100.0f;
            
            if (_currentMotion == FORWARD) {
                _calculatedPosition = _motorStartPosPercentage + projectedDelta;
                if (_calculatedPosition > 100.0f) _calculatedPosition = 100.0f;
            } else if (_currentMotion == REWIND) {
                _calculatedPosition = _motorStartPosPercentage - projectedDelta;
                if (_calculatedPosition < 0.0f) _calculatedPosition = 0.0f;
            }
            
            _currentPosition = (uint8_t)(_calculatedPosition + 0.5f); // Safe Rounding
        }
    }

    void processPulseStateMachine() {
        uint32_t currentTimeMs = millis();
        
        switch (_pulseState) {
            case IDLE:
                writeHardwarePins(STOP, false);
                _currentMotion = STOP;
                break;
                
            case RUNNING:
                writeHardwarePins(_currentMotion, false);
                
                if (_currentMode == DIRECT_MOTION) {
                    _targetPosition = _currentPosition;
                } 
                else if (_currentMode == PERCENTAGE_POSITION) {
                    if (_currentPosition == _targetPosition && _currentMotion != STOP) {
                        // Target reached; stop with a 100 ms dual-relay pulse.
                        initiateTransition(STOP);
                        _isSettling = true;
                    }
                }
                break;
                
            case STOP_PULSE:
                writeHardwarePins(STOP, true);

                if (currentTimeMs - _stateTimerMs >= _stopPulseMs) {
                    writeHardwarePins(STOP, false);
                    _currentMotion = STOP;
                    _isSettling = false;

                    if (_targetMotion != STOP) {
                        _currentMotion = _targetMotion;
                        _pulseState = RUNNING;
                        _stateTimerMs = currentTimeMs;
                        _motorStartTimestampMs = currentTimeMs;
                        _motorStartPosPercentage = _calculatedPosition;
                        writeHardwarePins(_currentMotion, false);
                    } else {
                        _pulseState = IDLE;
                        _stateTimerMs = currentTimeMs;
                    }
                }
                break;
        }
    }
};

#endif // CURTAIN_CONTROLLER_H