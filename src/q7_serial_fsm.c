/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Question 7 - Debouncing Serial RX Finite State Machine (FSM)
 * 
 * Student Name: Jaleel Gregory
 * ID NO:        816039976
 * Platform:     Arduino MKR Zero (Microchip ATSAMD21G18A ARM Cortex-M0+)
 * Compiler:     arm-none-eabi-gcc (GNU ARM Embedded Toolchain)
 * ============================================================================
 * Description:
 * Implements the debouncing serial RX FSM from Lab 1, Page 4 (Question 7).
 * 
 * Transformation Strategy Chosen (Williams Chapter 6):
 * - Switch-case transformation technique.
 * - Comments document state definitions, synchronous system tick handling,
 *   and debounce logic.
 * ============================================================================
 */

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_OFF = 0,
    STATE_ON  = 1
} FSM_State_t;

#define SYSTEM_TICK_MS       10
#define DEBOUNCE_INTERVAL_MS 500

static FSM_State_t current_state = STATE_OFF;
static char last_char = '\0';
static uint32_t elapsed_ms = DEBOUNCE_INTERVAL_MS;

/**
 * FSM transition function called strictly on system tick
 * Input: has_new_char (flag), rx_char (received character)
 * Output: returns updated LED state (0 = OFF, 1 = ON)
 */
uint8_t fsm_step(bool has_new_char, char rx_char) {
    if (elapsed_ms < 10000) {
        elapsed_ms += SYSTEM_TICK_MS;
    }

    if (has_new_char) {
        bool same_char = (rx_char == last_char);
        bool interval_exceeded = (elapsed_ms >= DEBOUNCE_INTERVAL_MS);

        switch (current_state) {
            case STATE_OFF:
                if (same_char && !interval_exceeded) {
                    /* Ignored / Debounced: stay in STATE_OFF */
                } else {
                    /* Valid transition: toggle to STATE_ON */
                    current_state = STATE_ON;
                }
                break;

            case STATE_ON:
                if (same_char && !interval_exceeded) {
                    /* Ignored / Debounced: stay in STATE_ON */
                } else {
                    /* Valid transition: toggle to STATE_OFF */
                    current_state = STATE_OFF;
                }
                break;

            default:
                current_state = STATE_OFF;
                break;
        }

        last_char = rx_char;
        elapsed_ms = 0;
    }

    return (current_state == STATE_ON) ? 1 : 0;
}

