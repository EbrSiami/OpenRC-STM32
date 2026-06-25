/**
 * @file buzzer.cpp
 * @author Александр Королёв
 * @brief buzzer handling engine
 * @version 4.0.1
 * @date 2026-04-18
 */

#include "Buzzer.h"

// ---------- Patterns ----------
// 1- User Interface Sounds (UI)
const BeepStep PATTERN_NAV_STEPS[]         = { {15, true},  {0, false} };
const BeepStep PATTERN_CLICK_STEPS[]       = { {30, true},  {0, false} };
const BeepStep PATTERN_CANCEL_STEPS[]      = { {60, true},  {40, false}, {30, true}, {0, false} };
const BeepStep PATTERN_CONFIRM_STEPS[]     = { {40, true},  {40, false}, {80, true}, {0, false} };

// 2- Trim Sounds
const BeepStep PATTERN_TRIM_STEP_STEPS[]   = { {20, true},  {0, false} };
const BeepStep PATTERN_TRIM_CENTER_STEPS[] = { {30, true},  {40, false}, {30, true}, {0, false} };
const BeepStep PATTERN_TRIM_LIMIT_STEPS[]  = { {150, true}, {0, false} };

// 3- System and Alarm Sounds
const BeepStep PATTERN_STARTUP_STEPS[]     = { {100, true}, {50, false}, {100, true}, {50, false}, {200, true}, {0, false} };
const BeepStep PATTERN_LOW_BATT_STEPS[]    = { {150, true}, {100, false}, {150, true}, {100, false}, {150, true}, {0, false} }; // سه بوق (SOS)
const BeepStep PATTERN_TIMER_START_STEPS[] = { {100, true}, {0, false} };
const BeepStep PATTERN_TIMER_DONE_STEPS[]  = { {400, true}, {100, false}, {400, true}, {100, false}, {800, true}, {0, false} };
const BeepStep PATTERN_TIMER_1MIN_STEPS[]  = { {150, true}, {60, false}, {300, true}, {0, false} };
const BeepStep PATTERN_TIMER_30SEC_STEPS[] = { {600, true}, {0, false} };
const BeepStep PATTERN_TIMER_TICK_STEPS[]  = { {40, true}, {0, false} };
const BeepStep PATTERN_ERROR_STEPS[]       = { {200, true}, {100, false}, {200, true}, {0, false} };

const BeepPattern BP_NAV        = { PATTERN_NAV_STEPS };
const BeepPattern BP_CLICK      = { PATTERN_CLICK_STEPS };
const BeepPattern BP_CANCEL     = { PATTERN_CANCEL_STEPS };
const BeepPattern BP_CONFIRM    = { PATTERN_CONFIRM_STEPS };
const BeepPattern BP_TRIM_STEP  = { PATTERN_TRIM_STEP_STEPS };
const BeepPattern BP_TRIM_CENTER= { PATTERN_TRIM_CENTER_STEPS };
const BeepPattern BP_TRIM_LIMIT = { PATTERN_TRIM_LIMIT_STEPS };
const BeepPattern BP_STARTUP    = { PATTERN_STARTUP_STEPS };
const BeepPattern BP_LOW_BATT   = { PATTERN_LOW_BATT_STEPS };
const BeepPattern BP_TIMER_START= { PATTERN_TIMER_START_STEPS };
const BeepPattern BP_TIMER_DONE = { PATTERN_TIMER_DONE_STEPS };
const BeepPattern BP_TIMER_1MIN  = { PATTERN_TIMER_1MIN_STEPS };
const BeepPattern BP_TIMER_30SEC = { PATTERN_TIMER_30SEC_STEPS };
const BeepPattern BP_TIMER_TICK  = { PATTERN_TIMER_TICK_STEPS };
const BeepPattern BP_ERROR      = { PATTERN_ERROR_STEPS };

// ---------- Manager ----------
BuzzerManager buzzer;

void BuzzerManager::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    stopNow();
}

void BuzzerManager::update(bool buzzerEnabled) {
    unsigned long now = millis();

    if (!_active) {
        popNext();
        if (!_active) { digitalWrite(_pin, LOW); return; }
    }

    if (!_currentForce && !buzzerEnabled) { stopNow(); return; }

    const BeepStep& st = _currentPattern->steps[_stepIndex];
    if (st.durationMs == 0) { stopNow(); return; }

    if (now - _stepStartMs >= st.durationMs) {
        _stepIndex++;
        const BeepStep& next = _currentPattern->steps[_stepIndex];
        _stepStartMs = now;
        if (next.durationMs == 0) stopNow();
        else digitalWrite(_pin, next.on ? HIGH : LOW);
    }
}

void BuzzerManager::enqueue(const BeepPattern* pattern, BeepPriority prio, bool force) {
    if (!pattern) return;
    if (isFull()) {
        int idx = findLowestPriorityIndex();
        if (idx >= 0 && _queue[idx].priority <= prio) removeAt(idx);
        else return;
    }
    _queue[_tail] = { pattern, prio, force };
    _tail = (_tail + 1) % QUEUE_SIZE;
    _count++;
}

void BuzzerManager::playImmediate(const BeepPattern* pattern, BeepPriority prio, bool force) {
    if (!pattern) return;
    if (_active && prio >= _currentPriority) stopNow();
    enqueue(pattern, prio, force);
}

bool BuzzerManager::isBusy() const { return _active || _count > 0; }

void BuzzerManager::clearQueue() { _head = _tail = _count = 0; }

void BuzzerManager::stopNow() {
    digitalWrite(_pin, LOW);
    _active = false;
    _currentPattern = nullptr;
    _stepIndex = 0;
    _currentForce = false;
    _currentPriority = BEEP_PRIO_LOW;
}

bool BuzzerManager::isFull() const { return _count >= QUEUE_SIZE; }

void BuzzerManager::popNext() {
    if (_count == 0) return;
    BeepRequest r = _queue[_head];
    _head = (_head + 1) % QUEUE_SIZE; _count--;
    _currentPattern = r.pattern;
    _currentForce = r.force;
    _currentPriority = r.priority;
    _stepIndex = 0;
    _stepStartMs = millis();
    _active = true;

    const BeepStep& st = _currentPattern->steps[_stepIndex];
    if (st.durationMs == 0) stopNow();
    else digitalWrite(_pin, st.on ? HIGH : LOW);
}

int BuzzerManager::findLowestPriorityIndex() {
    if (_count == 0) return -1;
    uint8_t minIdx = _head;
    BeepPriority minP = _queue[_head].priority;
    for (uint8_t i = 0; i < _count; i++) {
        uint8_t cur = (_head + i) % QUEUE_SIZE;
        if (_queue[cur].priority < minP) {
            minP = _queue[cur].priority;
            minIdx = cur;
        }
    }
    return minIdx;
}

void BuzzerManager::removeAt(int index) {
    if (_count == 0 || index < 0) return;
    uint8_t idx = index;
    while (idx != _tail) {
        uint8_t next = (idx + 1) % QUEUE_SIZE;
        if (next == _tail) break;
        _queue[idx] = _queue[next];
        idx = next;
    }
    _tail = (_tail + QUEUE_SIZE - 1) % QUEUE_SIZE;
    _count--;
}

// ---------- API ----------
void playBeepEvent(BeepEvent e) {
    switch (e) {
        case EVT_NAV:         buzzer.enqueue(&BP_NAV, BEEP_PRIO_LOW, false); break;
        case EVT_CLICK:       buzzer.enqueue(&BP_CLICK, BEEP_PRIO_LOW, false); break;
        case EVT_CANCEL:      buzzer.enqueue(&BP_CANCEL, BEEP_PRIO_LOW, false); break;
        case EVT_CONFIRM:     buzzer.enqueue(&BP_CONFIRM, BEEP_PRIO_MEDIUM, false); break;
        case EVT_TRIM_STEP:   buzzer.enqueue(&BP_TRIM_STEP, BEEP_PRIO_LOW, false); break;
        case EVT_TRIM_CENTER: buzzer.enqueue(&BP_TRIM_CENTER, BEEP_PRIO_MEDIUM, false); break;
        case EVT_TRIM_LIMIT:  buzzer.enqueue(&BP_TRIM_LIMIT, BEEP_PRIO_MEDIUM, false); break;
        case EVT_STARTUP:     buzzer.enqueue(&BP_STARTUP, BEEP_PRIO_HIGH, true); break;
        case EVT_TIMER_START: buzzer.enqueue(&BP_TIMER_START, BEEP_PRIO_MEDIUM, false); break;
        case EVT_TIMER_DONE:  buzzer.playImmediate(&BP_TIMER_DONE, BEEP_PRIO_HIGH, true); break;
        case EVT_LOW_BATTERY: buzzer.playImmediate(&BP_LOW_BATT, BEEP_PRIO_HIGH, true); break;
        case EVT_ERROR:       buzzer.playImmediate(&BP_ERROR, BEEP_PRIO_HIGH, true); break;
        case EVT_TIMER_1MIN:  buzzer.enqueue(&BP_TIMER_1MIN, BEEP_PRIO_MEDIUM, false); break;
        case EVT_TIMER_30SEC: buzzer.enqueue(&BP_TIMER_30SEC, BEEP_PRIO_MEDIUM, false); break;
        case EVT_TIMER_TICK:  buzzer.enqueue(&BP_TIMER_TICK, BEEP_PRIO_MEDIUM, false); break;

    }
}