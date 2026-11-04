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
 * FSM Transformation Strategy Chosen (Williams Chapter 6):
 * - Technique: switch-case transformation.
 * - Justification: Williams Ch. 6 recommends switch-case architectures for 
 *   deterministic, event-driven state transitions with clean separation of 
 *   state entry/exit logic and low memory footprint on microcontrollers.
 * 
 * Timing and Transition Rules:
 * - System tick period: 10 ms (factor of 500 ms).
 * - FSM transitions execute strictly synchronized on system ticks.
 * - An elapsed-time counter tracks the interval since the last valid character.
 * - If the same character arrives within < 500 ms, it is debounced and ignored.
 * - If a different character arrives, or the same character arrives after >= 500 ms,
 *   the FSM toggles state: OFF -> ON (LED ON) or ON -> OFF (LED OFF).
 * ============================================================================
 */

#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>

/* FSM States */
typedef enum {
    STATE_OFF = 0,
    STATE_ON  = 1
} FSM_State_t;

/* System tick configuration (10 ms is an exact factor of 500 ms: 50 ticks = 500 ms) */
#define SYSTEM_TICK_MS       10
#define DEBOUNCE_INTERVAL_MS 500

/* FSM State Variables */
static FSM_State_t current_state = STATE_OFF;
static char last_char = '\0';
static uint32_t elapsed_ms = DEBOUNCE_INTERVAL_MS; // Start primed so 1st char triggers
static bool char_pending = false;
static char rx_char = '\0';

/**
 * Configure hardware peripherals (LED pin and Serial interface)
 */
void fsm_init(void) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Initial state: OFF
    current_state = STATE_OFF;
    last_char = '\0';
    elapsed_ms = DEBOUNCE_INTERVAL_MS;
    char_pending = false;
}

/**
 * Poll serial receiver to detect arriving characters
 * Returns true if a valid printable character has arrived.
 */
bool fsm_serial_check_rx(char *out_char) {
    if (Serial.available() > 0) {
        int c = Serial.read();
        // Ignore carriage return / newline whitespace formatting
        if (c != '\r' && c != '\n' && c >= 0) {
            *out_char = (char)c;
            return true;
        }
    }
    return false;
}

/**
 * Execute FSM step (Synchronized strictly to the system tick)
 */
void fsm_update_on_tick(void) {
    // 1. Advance the tick timer (saturate to prevent overflow)
    if (elapsed_ms < 10000) {
        elapsed_ms += SYSTEM_TICK_MS;
    }

    // 2. Process incoming character event if present
    if (char_pending) {
        bool same_char = (rx_char == last_char);
        bool interval_exceeded = (elapsed_ms >= DEBOUNCE_INTERVAL_MS);

        char debug_buf[128];
        snprintf(debug_buf, sizeof(debug_buf), 
                 "[Tick %lu ms] RX: '%c' | last: '%c' | same: %d | dt: %lu ms | State: %s -> ",
                 (unsigned long)millis(), rx_char, (last_char == '\0' ? '?' : last_char),
                 same_char ? 1 : 0, (unsigned long)elapsed_ms,
                 (current_state == STATE_ON ? "ON" : "OFF"));
        Serial.print(debug_buf);

        // Evaluate FSM using switch-case transformation (Williams Ch 6)
        switch (current_state) {
            case STATE_OFF:
                if (same_char && !interval_exceeded) {
                    // Ignored / Debounced: self-loop (same_char && interval < 500ms)
                    Serial.println("OFF (Debounced / Ignored)");
                } else {
                    // Transition: (!same_char) OR (same_char && interval >= 500ms)
                    current_state = STATE_ON;
                    digitalWrite(LED_BUILTIN, HIGH); // Turn LED ON
                    Serial.println("ON (LED ACTIVATED)");
                }
                break;

            case STATE_ON:
                if (same_char && !interval_exceeded) {
                    // Ignored / Debounced: self-loop (same_char && interval < 500ms)
                    Serial.println("ON (Debounced / Ignored)");
                } else {
                    // Transition: (!same_char) OR (same_char && interval >= 500ms)
                    current_state = STATE_OFF;
                    digitalWrite(LED_BUILTIN, LOW); // Turn LED OFF
                    Serial.println("OFF (LED DEACTIVATED)");
                }
                break;

            default:
                current_state = STATE_OFF;
                digitalWrite(LED_BUILTIN, LOW);
                break;
        }

        // Update tracking history and reset elapsed counter
        last_char = rx_char;
        elapsed_ms = 0;
        char_pending = false;
    }
}

#ifndef UNIT_TEST
void setup(void) {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait up to 3 seconds for USB Serial Monitor
    }

    fsm_init();

    Serial.println("\r\n=======================================================");
    Serial.println(" ECNG3006 Lab 1: Question 7 - Debouncing Serial RX FSM");
    Serial.println(" Student: Jaleel Gregory | ID: 816039976");
    Serial.println(" Implementation: switch-case transformation (Williams Ch 6)");
    Serial.println(" System Tick: 10 ms | Debounce Interval: 500 ms");
    Serial.println(" Target: Arduino MKR Zero (LED_BUILTIN Pin 32 / PB08)");
    Serial.println("=======================================================");
    Serial.println("Type characters into the Serial Monitor to test debouncing:\r\n");
}

void loop(void) {
    static uint32_t last_tick_time = 0;

    // Buffer incoming serial characters
    char c;
    if (fsm_serial_check_rx(&c)) {
        rx_char = c;
        char_pending = true;
    }

    // Enforce synchronous system tick execution
    uint32_t current_time = millis();
    if (current_time - last_tick_time >= SYSTEM_TICK_MS) {
        last_tick_time = current_time;
        fsm_update_on_tick();
    }
}
#endif
