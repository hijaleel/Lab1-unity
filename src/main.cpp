/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Questions 7 & 8 - Serial RX Debounce FSM & Oscilloscope UART TX
 * 
 * Student Name: Jaleel Gregory
 * ID NO:        816039976
 * Platform:     Arduino MKR Zero (Microchip ATSAMD21G18A ARM Cortex-M0+)
 * Compiler:     arm-none-eabi-gcc (GNU ARM Embedded Toolchain)
 * ============================================================================
 * Description:
 * 1. Question 7: Debouncing Serial RX FSM using switch-case transformation.
 * 2. Question 8: Bare-metal sam.h UART transmitter (SERCOM5) on Pin 14 (PB22).
 *    - Transmits standard calibration character 'U' (0x55 = 0b01010101) 
 *      periodically every 100 ms to produce a deterministic square wave for 
 *      oscilloscope bit timing and inter-character interval capture.
 * ============================================================================
 */

#include <Arduino.h>
#include <sam.h>
#include <stdint.h>
#include <stdio.h>

/* FSM States */
typedef enum {
    STATE_OFF = 0,
    STATE_ON  = 1
} FSM_State_t;

#define SYSTEM_TICK_MS       10
#define DEBOUNCE_INTERVAL_MS 500

/* FSM State Variables */
static FSM_State_t current_state = STATE_OFF;
static char last_char = '\0';
static uint32_t elapsed_ms = DEBOUNCE_INTERVAL_MS;
static bool char_pending = false;
static char rx_char = '\0';

/**
 * Configure Bare-Metal sam.h USART on SERCOM5:
 * Pin 14 on Arduino MKR Zero is PB22, which is SERCOM5 PAD[2] (Function D).
 */
void samd21_uart_tx_init_baremetal(void) {
    // 1. Enable APBC clock for SERCOM5 in Power Manager
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;

    // 2. Route Generic Clock Generator 0 (48 MHz) to GCLK_SERCOM5_CORE
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_SERCOM5_CORE_Val) |
                        GCLK_CLKCTRL_GEN(GCLK_CLKCTRL_GEN_GCLK0_Val) |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    // 3. Configure Pin PB22 (Pin 14) for Peripheral Function D (SERCOM5 PAD[2])
    PORT->Group[1].PINCFG[22].bit.PMUXEN = 1;
    PORT->Group[1].PMUX[11].bit.PMUXE = PORT_PMUX_PMUXE_D_Val; // Even pin PB22 -> PMUXE

    // 4. Reset SERCOM5
    SERCOM5->USART.CTRLA.bit.SWRST = 1;
    while (SERCOM5->USART.CTRLA.bit.SWRST || SERCOM5->USART.SYNCBUSY.bit.SWRST);

    // 5. Configure USART Mode, Internal Clock, Asynchronous arithmetic baud rate,
    //    TXPO: PAD[2] for TX (TXPO = 1 maps TX to PAD[2])
    SERCOM5->USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
                               SERCOM_USART_CTRLA_DORD |                 // LSB first
                               SERCOM_USART_CTRLA_TXPO(1);              // PAD[2] = TX

    // 6. 8-N-1: 8 data bits, no parity, 1 stop bit
    SERCOM5->USART.CTRLB.reg = SERCOM_USART_CTRLB_TXEN |                 // Enable TX
                               SERCOM_USART_CTRLB_CHSIZE(0);             // 8 bits

    // 7. Calculate 115200 Baud Rate for 48 MHz GCLK:
    // BAUD = 65536 * (1 - 16 * (115200 / 48000000)) = 63019
    SERCOM5->USART.BAUD.reg = 63019;

    // 8. Enable USART
    SERCOM5->USART.CTRLA.bit.ENABLE = 1;
    while (SERCOM5->USART.SYNCBUSY.bit.ENABLE);
}

/**
 * Transmit a single byte over bare-metal SERCOM5 USART with busy-wait
 */
void samd21_uart_send_char(char c) {
    // Wait until Data Register Empty (DRE) flag is set
    while (!(SERCOM5->USART.INTFLAG.bit.DRE));
    SERCOM5->USART.DATA.reg = (uint16_t)c;
}

/**
 * Configure hardware peripherals (LED pin and FSM state)
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
 */
bool fsm_serial_check_rx(char *out_char) {
    if (Serial.available() > 0) {
        int c = Serial.read();
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
    if (elapsed_ms < 10000) {
        elapsed_ms += SYSTEM_TICK_MS;
    }

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

        // Also echo character out over bare-metal Pin 14 TX line
        samd21_uart_send_char(rx_char);

        // Evaluate FSM using switch-case transformation (Williams Ch 6)
        switch (current_state) {
            case STATE_OFF:
                if (same_char && !interval_exceeded) {
                    Serial.println("OFF (Debounced / Ignored)");
                } else {
                    current_state = STATE_ON;
                    digitalWrite(LED_BUILTIN, HIGH);
                    Serial.println("ON (LED ACTIVATED)");
                }
                break;

            case STATE_ON:
                if (same_char && !interval_exceeded) {
                    Serial.println("ON (Debounced / Ignored)");
                } else {
                    current_state = STATE_OFF;
                    digitalWrite(LED_BUILTIN, LOW);
                    Serial.println("OFF (LED DEACTIVATED)");
                }
                break;

            default:
                current_state = STATE_OFF;
                digitalWrite(LED_BUILTIN, LOW);
                break;
        }

        last_char = rx_char;
        elapsed_ms = 0;
        char_pending = false;
    }
}

#ifndef UNIT_TEST
void setup(void) {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait up to 3s for USB Serial Monitor
    }

    fsm_init();

    // Initialize bare-metal sam.h UART on Pin 14 (PB22 / SERCOM5)
    samd21_uart_tx_init_baremetal();

    Serial.println("\r\n=======================================================");
    Serial.println(" ECNG3006 Lab 1: Question 7 & 8 - FSM & Oscilloscope TX");
    Serial.println(" Student: Jaleel Gregory | ID: 816039976");
    Serial.println(" Bare-Metal UART: SERCOM5 TX on Pin 14 (PB22)");
    Serial.println(" Calibration Pattern: 'U' (0x55) broadcast every 100 ms");
    Serial.println("=======================================================\r\n");
}

void loop(void) {
    static uint32_t last_tick_time = 0;
    static uint32_t last_scope_tx_time = 0;

    // Buffer incoming serial characters from monitor
    char c;
    if (fsm_serial_check_rx(&c)) {
        rx_char = c;
        char_pending = true;
    }

    // Question 8: Broadcast calibration character 'U' (0x55) every 100 ms on Pin 14 TX
    uint32_t current_time = millis();
    if (current_time - last_scope_tx_time >= 100) {
        last_scope_tx_time = current_time;
        samd21_uart_send_char('U'); // Pure bare-metal transmit on Pin 14
    }

    // Question 7: Synchronous 10 ms FSM step
    if (current_time - last_tick_time >= SYSTEM_TICK_MS) {
        last_tick_time = current_time;
        fsm_update_on_tick();
    }
}
#endif
