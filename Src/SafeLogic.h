#ifndef SAFE_LOGIC_H_
#define SAFE_LOGIC_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool unlock_requested;
    bool button_raw;
    bool button_pressed;
    bool button_initialized;
    bool pot_armed;
    bool knock_raw;
    bool prev_unlock;
    unit8_t knock_count;
    uint32_t button_changed_at;
    unit32_t last_knock_at;
    unit32_t knock_ignore_until;
    uint16_t pot;
} SafeState;

void InitializeSafe(SafeState *state, uint32_t now);
void SampleSafe(SafeState *state, uint32_t now, bool button_pressed, uint16_t pot);
bool ProcessSafeCommand(SafeState *state, char command);
void LockSafe(SafeState *state);

//Knock from sound sensor - returns true on the thrid knock, when it arrives and toggles the lock request
bool KnockSafe(SafeState *state, uint32_t now, bool sound_active);

#endif
