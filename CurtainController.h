#ifndef CURTAIN_CONTROLLER_H
#define CURTAIN_CONTROLLER_H

#include <Arduino.h>

class CurtainController {
public:
    enum Motion : uint8_t {
        STOP = 0,
        FORWARD, // Increments position towards 100%
        REWIND   // Decrements position towards 0%
    };

    enum ControlMode : uint8_t {
        DIRECT_MOTION = 0,
        PERCENTAGE_POSITION
    };

    // Constructor
    CurtainController(uint8_t enablePin, uint8_t directionPin,
                      bool enableActiveHigh = false,
                      bool directionForwardHigh = false,
                      uint32_t fullTravelTimeMs = 30000UL,
                      uint32_t endpointResyncHoldMs = 2000UL)
        : _enablePin(enablePin),
          _directionPin(directionPin),
          _enableActiveHigh(enableActiveHigh),
          _directionForwardHigh(directionForwardHigh),
          _fullTravelTimeMs(fullTravelTimeMs),
          _endpointResyncHoldMs(endpointResyncHoldMs),
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
          _endpointTimerTriggered(false),
          _lastRawDMXDirect(255),  // Initialized out-of-band to force first-run processing
          _lastRawDMXPercent(255) // Initialized out-of-band to force first-run processing
          {}

    // Core Methods
    void begin() {
        pinMode(_enablePin, OUTPUT);
        pinMode(_directionPin, OUTPUT);
        
        // Safely pull pins to their logical offline/inactive state
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
                // Keep endpoint targets latched without issuing a stop pulse.
                if (_targetPosition == 100 || _targetPosition == 0) {
                    if (_currentMotion == STOP) {
                        // Already parked at the absolute edge boundary.
                        _targetPosition = _currentPosition;
                    }
                } 
                // Intermediate positions (1%-99%) execution stop rule
                else if (_currentMotion != STOP) {
                    initiateTransition(STOP);
                    _isSettling = true; // Settle Lock engaged until sequence un-jams relays
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
        PULSE_START,
        RUNNING,
        PULSE_STOP,
        PULSE_REVERSE
    };

    // Hardware Pins
    uint8_t _enablePin;
    uint8_t _directionPin;
    
    // Logic Configuration Flags
    bool _enableActiveHigh;
    bool _directionForwardHigh;

    // Timing Configuration Constraints
    uint32_t _fullTravelTimeMs;
    uint32_t _endpointResyncHoldMs;

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
    bool _endpointTimerTriggered;

    // Change Detection History Trackers
    byte _lastRawDMXDirect;
    byte _lastRawDMXPercent;

    // Private Helper Functions
    void writeHardwarePins(Motion motionState, bool enableMotor) {
        bool pinDirectionValue = false;
        if (motionState == FORWARD) {
            pinDirectionValue = _directionForwardHigh;
        } else if (motionState == REWIND) {
            pinDirectionValue = !_directionForwardHigh;
        } else {
            pinDirectionValue = false; 
        }
        
        bool pinEnableValue = enableMotor ? _enableActiveHigh : !_enableActiveHigh;
        
        digitalWrite(_directionPin, pinDirectionValue ? HIGH : LOW);
        digitalWrite(_enablePin, pinEnableValue ? HIGH : LOW);
    }

    void initiateTransition(Motion nextMotion) {
        if (_targetMotion == nextMotion) return;
        
        _targetMotion = nextMotion;
        
        if (_pulseState == IDLE && _targetMotion != STOP) {
            _pulseState = PULSE_START;
            _stateTimerMs = millis();
            
            // Log structural step reference point before motor begins applying mechanical energy
            _motorStartTimestampMs = _stateTimerMs; 
            _motorStartPosPercentage = _calculatedPosition;
        } else if (_pulseState == RUNNING && _targetMotion != _currentMotion) {
            _pulseState = PULSE_STOP;
            _stateTimerMs = millis();
        }
    }

    void updatePositionTracking() {
        if (_currentMotion != STOP && _pulseState == RUNNING) {
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
        const uint32_t PULSE_DURATION_MS = 500UL;
        
        switch (_pulseState) {
            case IDLE:
                writeHardwarePins(STOP, false);
                _currentMotion = STOP;
                break;
                
            case PULSE_START:
                _currentMotion = _targetMotion;
                writeHardwarePins(_currentMotion, true);
                
                if (currentTimeMs - _stateTimerMs >= PULSE_DURATION_MS) {
                    _pulseState = RUNNING;
                    // Reset track timing window baseline specifically for runtime updates
                    _motorStartTimestampMs = currentTimeMs;
                }
                break;
                
            case RUNNING:
                writeHardwarePins(_currentMotion, true);
                
                if (_currentMode == DIRECT_MOTION) {
                    _targetPosition = _currentPosition;
                } 
                else if (_currentMode == PERCENTAGE_POSITION) {
                    if ((_currentMotion == FORWARD && _targetPosition == 100 && _currentPosition == 100) ||
                        (_currentMotion == REWIND && _targetPosition == 0 && _currentPosition == 0)) {
                        // Endpoint target already reached; keep the target synced without sending a stop pulse.
                        _targetPosition = _currentPosition;
                    }
                }
                break;
                
            case PULSE_STOP:
                {
                    Motion counterBrakeDirection = (_currentMotion == FORWARD) ? REWIND : FORWARD;
                    writeHardwarePins(counterBrakeDirection, true);
                }
                
                if (currentTimeMs - _stateTimerMs >= PULSE_DURATION_MS) {
                    writeHardwarePins(STOP, false); 
                    _currentMotion = STOP;
                    _pulseState = PULSE_REVERSE;
                    _stateTimerMs = currentTimeMs;
                }
                break;
                
            case PULSE_REVERSE:
                writeHardwarePins(STOP, false);
                
                if (currentTimeMs - _stateTimerMs >= PULSE_DURATION_MS) {
                    _isSettling = false; 
                    
                    if (_targetMotion != STOP) {
                        _pulseState = PULSE_START; 
                    } else {
                        _pulseState = IDLE;
                    }
                    _stateTimerMs = currentTimeMs;
                }
                break;
        }
    }
};

#endif // CURTAIN_CONTROLLER_H