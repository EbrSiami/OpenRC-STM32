/**
 * @file buzzer.h
 * @author Александр Королёв
 * @brief buzzer handling engine
 * @version 4.0.1
 * @date 2026-04-18
 */

#pragma once
#include <Arduino.h>

enum BeepPriority : uint8_t {
    BEEP_PRIO_LOW = 0,
    BEEP_PRIO_MEDIUM = 1,
    BEEP_PRIO_HIGH = 2
};

enum BeepEvent : uint8_t {
    EVT_STARTUP,      // Radio turns on
    EVT_NAV,          // navigiation in menus
    EVT_CLICK,        // enter the menu / select
    EVT_CANCEL,       // turns back
    EVT_CONFIRM,      // saving settings
    EVT_TRIM_STEP,    // trim change one step
    EVT_TRIM_CENTER,  // trim reachs the center
    EVT_TRIM_LIMIT,   // trim reachs the end
    EVT_TIMER_START,  // start the timer
    EVT_TIMER_DONE,   // end the timer
    EVT_LOW_BATTERY,  // low battery
    EVT_ERROR,        // general error
    EVT_TIMER_1MIN,   // 1 min left timer
    EVT_TIMER_30SEC,  // 30 second left timer
    EVT_TIMER_TICK    // 10 second left timer ticks
};

struct BeepStep { uint16_t durationMs; bool on; };
struct BeepPattern { const BeepStep* steps; };
struct BeepRequest { const BeepPattern* pattern; BeepPriority priority; bool force; };

class BuzzerManager {
public:
    void begin(uint8_t pin);
    void update(bool buzzerEnabled);
    void enqueue(const BeepPattern* pattern, BeepPriority prio, bool force=false);
    void playImmediate(const BeepPattern* pattern, BeepPriority prio, bool force=true);
    bool isBusy() const;
    void clearQueue();
    void stopNow();

private:
    static const uint8_t QUEUE_SIZE = 8;
    uint8_t _pin = 255;

    BeepRequest _queue[QUEUE_SIZE];
    uint8_t _head = 0, _tail = 0, _count = 0;

    bool _active = false;
    const BeepPattern* _currentPattern = nullptr;
    uint8_t _stepIndex = 0;
    unsigned long _stepStartMs = 0;
    bool _currentForce = false;
    BeepPriority _currentPriority = BEEP_PRIO_LOW;

    bool isFull() const;
    void popNext();
    int findLowestPriorityIndex();
    void removeAt(int index);
};

extern BuzzerManager buzzer;
void playBeepEvent(BeepEvent e);