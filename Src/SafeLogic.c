/*
 * Breadboard lock request logic; the caller supplies button and ADC readings.
 */
#include "SafeLogic.h"

#define BUTTON_DEBOUNCE_MS 25U
#define POT_LOW_MAX 1228U
#define POT_HIGH_MIN 2867U

#define KNOCKS_NEEDED 3U
#define KNOCK_LOCKOUT_MS 150U   // ignore ringing right after a knock 
#define KNOCK_GAP_MAX_MS 1500U  // too long between knocks: start over
#define KNOCK_SETTLE_MS 3000U   // deaf after a lock change (solenoid click, recorder playing)

void InitializeSafe(SafeState *state, uint32_t now) {
    *state = (SafeState){0};
    state->button_changed_at = now;
}

void LockSafe(SafeState *state) {
    state->unlock_requested = false;
}

void SampleSafe(SafeState *state, uint32_t now, bool button_pressed, uint16_t pot) {
    bool toggle = false;
    state->pot = pot;

    /* A new press counts only after the input has remained stable for 25 ms. */
    if (!state->button_initialized) {
        /* An already-held button at boot is not a new press. */
        state->button_raw = button_pressed;
        state->button_pressed = button_pressed;
        state->button_initialized = true;
    } else if (button_pressed != state->button_raw) {
        state->button_raw = button_pressed;
        state->button_changed_at = now;
    } else if (state->button_pressed != state->button_raw &&
               (uint32_t)(now - state->button_changed_at) >= BUTTON_DEBOUNCE_MS) {
        state->button_pressed = state->button_raw;
        toggle = state->button_pressed;
    }

    /* The pot must return to the low region before another high turn counts. */
    if (pot <= POT_LOW_MAX) {
        state->pot_armed = true;
    } else if (pot >= POT_HIGH_MIN && state->pot_armed) {
        state->pot_armed = false;
        toggle = true;
    }

    if (toggle) {
        state->unlock_requested = !state->unlock_requested;
    }
}

bool ProcessSafeCommand(SafeState *state, char command) {
    if (command == 'U' || command == 'u') {
        state->unlock_requested = !state->unlock_requested;
        return true;
    }
    if (command == 'L' || command == 'l') {
        LockSafe(state);
        return true;
    }
bool KnockSafe(SafeState *state, uint32_t now, bool sound_active) {
    // Any lock/unlock change (button, pot, phone, or knocks) makes the safe
       deaf for a few seconds: the solenoid click and the recorder's playback
       would otherwise be heard as knocks
    if (state->unlock_requested != state->prev_unlock) {
        state->prev_unlock = state->unlock_requested;
        state->knock_count = 0;
        state->knock_ignore_until = now + KNOCK_SETTLE_MS;
    }

    // half-finished attempt expires if the next knock takes too long
    if (state->knock_count > 0U &&
        (uint32_t)(now - state->last_knock_at) > KNOCK_GAP_MAX_MS) {
        state->knock_count = 0;
    }

    // knock is the sound input going from quiet to loud
    bool new_knock = sound_active && !state->knock_raw;
    state->knock_raw = sound_active;
    if (!new_knock) {
        return false;
    }
    if ((int32_t)(now - state->knock_ignore_until) < 0) {
        return false;
    }
    if (state->knock_count > 0U &&
        (uint32_t)(now - state->last_knock_at) < KNOCK_LOCKOUT_MS) {
        return false;  // same knock ringing, not a new one
    }

    state->knock_count++;
    state->last_knock_at = now;
    if (state->knock_count >= KNOCKS_NEEDED) {
        state->knock_count = 0;
        state->unlock_requested = !state->unlock_requested;
        return true;
    }
    return false;
}


