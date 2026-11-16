/*
 * ENGS 28 Lab 9 breadboard prototype
 *
 * A3 / PB1: potentiometer wiper, ADC channel 18
 * D4 / PB10: active-low toggle pushbutton with pull-up
 * D6 / PB5: solenoid transistor drive
 * D13 / PA5: on-board user LED
 * D10 / PB0: Bluefruit command/data mode
 * D1 / PB6, D0 / PB7, D5 / PB4, D3 / PB3: Bluefruit USART1 with flow control
 * PA2 / PA3: ST-LINK USB console on USART2
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ES28.h"
#include "ADC.h"
#include "ble.h"
#include "uart.h"
#include "uart1.h"
#include "SafeLogic.h"

#define BUTTON_BITNUM 10U
#define BUTTON_BITMASK (1U << BUTTON_BITNUM)
#define SOLENOID_BITNUM 5U
#define SOLENOID_BITMASK (1U << SOLENOID_BITNUM)
#define LED_BITNUM 5U
#define LED_BITMASK (1U << LED_BITNUM)
#define SOUND_BITNUM 15U
#define SOUND_BITMASK (1U << SOUND_BITNUM)
#define PLAY_BITNUM 7U
#define PLAY_BITMASK (1U << PLAY_BITNUM)
#define PLAY_PULSE_MS 50U
#define GPIO_BSRR_RESET_BIT_OFFSET 16U
#define SAMPLE_MS 20U
#define STATUS_MS 1000U
#define STATUS_LINE_CAPACITY 96U

/* Keep the default bench build's coil output off. */
#ifndef SOLENOID_HOLD_ENABLED
#define SOLENOID_HOLD_ENABLED 0U
#endif

/* Sound module output 
#ifndef SOUND_ACTIVE_HIGH
#define SOUND_ACTIVE_HIGH 1U
#endif

static void InitializeGPIO(void) {
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN;
    /* Keep the coil off while PB5 changes from its reset mode to output. */
    GPIOB->BSRR = SOLENOID_BITMASK << GPIO_BSRR_RESET_BIT_OFFSET;
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE5_Msk)
                 | (GPIO_OUTPUT << GPIO_MODER_MODE5_Pos);
    GPIOA->BSRR = LED_BITMASK << GPIO_BSRR_RESET_BIT_OFFSET;
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE5_Msk)
                 | (GPIO_OUTPUT << GPIO_MODER_MODE5_Pos);
    /* The toggle button pulls PB10 low when pressed. */
    GPIOB->MODER &= ~GPIO_MODER_MODE10_Msk;
    GPIOB->PUPDR = (GPIOB->PUPDR & ~GPIO_PUPDR_PUPD10_Msk)
                 | (GPIO_PULLUP << GPIO_PUPDR_PUPD10_Pos);
    /* The sound module is in PA15 - mode 00, no pull; PA15 doesnt reset to input */
    GPIOA->MODER &= ~GPIO_MODER_MODE15_Msk;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD15_Msk;

    /*recorder on PC7, starts low then gives output */
    GPIOC->ODR &= ~PLAY_BITMASK;
    GPIOC->MODER = (GPIOC->MODER & ~GPIO_MODER_MODE7_Msk) | (GPIO_OUTPUT << GPIO_PUPDR_MODE7_Pos);
    
}

static void ApplyOutputs(const SafeState *state) {
#if SOLENOID_HOLD_ENABLED > 0U
    /* Hold PB5 high until the next lock request, as in the supplied driver example. */
    GPIOB->BSRR = state->unlock_requested ? SOLENOID_BITMASK
                                         : (SOLENOID_BITMASK << GPIO_BSRR_RESET_BIT_OFFSET);
#else
    GPIOB->BSRR = SOLENOID_BITMASK << GPIO_BSRR_RESET_BIT_OFFSET;
#endif
    GPIOA->BSRR = state->unlock_requested ? LED_BITMASK
                                         : (LED_BITMASK << GPIO_BSRR_RESET_BIT_OFFSET);
}
static bool SoundActive (void) {
    bool high = (GPIOA->IDR & SOUND_BITMASK) != OU;
    return (SOUND_ACTIVE_HIGH >  OU) ? high : !high;
}
/* recorder playback, raise playe then drop a little later, loop doesnt wait
static bool play_active = false;
static uint32_t play_started = OU;

static void StartPlayback(uint32_t now) {
    GPIOC->ODR |= PLAY_BITMASK;
    play_active = true;
    play_started = now;
}

static void ServicePlayback(uint32_t now) {
    if (play_active && (uint32_t)(now - play_started) >= PLAY_PULSE_MS) {
        GPIOC->ODR &= ~PLAY_BITMASK;
        play_active = false;
    }
}

static void SendStatus(const SafeState *state) {
    char line[STATUS_LINE_CAPACITY] = {0};
#if SOLENOID_HOLD_ENABLED > 0U
    const char *solenoid_status = state->unlock_requested ? "ON" : "OFF";
#else
    const char *solenoid_status = "DISABLED";
#endif
    (void)snprintf(line, sizeof(line), "POT=%u BUTTON=%s REQUEST=%s SOLENOID=%s\r\n",
                   (unsigned int)state->pot,
                   state->button_pressed ? "PRESSED" : "RELEASED",
                   state->unlock_requested ? "UNLOCK" : "LOCK",
                   solenoid_status);
    (void)BLE_Queue(line);
    printf("%s", line);
}

static void ProcessCommand(SafeState *state, int command) {
    if (command == '?') {
        SendStatus(state);
        return;
    }
    if (!ProcessSafeCommand(state, (char)command)) {
        return;
    }
    (void)BLE_Queue(state->unlock_requested ? "UNLOCK REQUESTED\r\n"
                                            : "LOCK REQUESTED\r\n");
    SendStatus(state);
}

int main(void) {
    SafeState state = {0};
    InitializeGPIO();
    InitializeSafe(&state, milliseconds());
    ApplyOutputs(&state);
    uart2_init();
    adc_init();
    bool ble_ready = BLE_Init();
    printf("Lab 9 prototype; BLE init %s; coil hold %s\r\n",
           ble_ready ? "OK" : "failed/absent",
#if SOLENOID_HOLD_ENABLED > 0U
           "enabled"
#else
           "disabled"
#endif
    );
    uint32_t last_sample = milliseconds();
    uint32_t last_status = last_sample;

    while (1) {
        uint32_t now = milliseconds();
        ApplyOutputs(&state);
        if (KnockSafe(&state, now, SoundActive())) {
            (void)BLE_Queue("3 KNOCKS\r\n");
            printf("3 knocks\r\n");
            ApplyOutputs(&state);
        }
        
        if ((uint32_t)(now - last_sample) >= SAMPLE_MS) {
            last_sample = now;
            int pot = adc_getValue();
            if (pot < 0) {
                LockSafe(&state);
            } else {
                bool pressed = (GPIOB->IDR & BUTTON_BITMASK) == 0U;
                SampleSafe(&state, now, pressed, (uint16_t)pot);
            }
            ApplyOutputs(&state);
        }

        if (uart1_RxAvail()) {
            ProcessCommand(&state, uart1_read());
            ApplyOutputs(&state);
        }
        if (uart2_RxAvail()) {
            ProcessCommand(&state, uart2_read());
            ApplyOutputs(&state);
        }
        if (state.unlock_requested && !was_unlocked) {
            StartPlayback(now);
        }
        was_unlocked = state.unlock_requested;
        ServicePlayback(now);
        
        if ((uint32_t)(now - last_status) >= STATUS_MS) {
            last_status = now;
            SendStatus(&state);
        }
        BLE_Poll();
    }
}
