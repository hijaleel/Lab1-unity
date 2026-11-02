/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Question 5 - Demonstration Cyclic Executive Code
 * 
 * Student Name: Jaleel
 * Platform:     Arduino MKR Zero (Microchip ATSAMD21G18A ARM Cortex-M0+)
 * Compiler:     arm-none-eabi-gcc (GNU ARM Embedded Toolchain)
 * ============================================================================
 * Description:
 * Adapted from Rob Williams, "Real-Time Systems Development", Section 4.7
 * ("Demonstration Cyclic Executive Code").
 * 
 * Key Platform Adaptations:
 * 1. Serial Print: Implemented using USB-CDC Serial communication at 115200 baud.
 * 2. Hardware Timer & ISR: Configured SysTick / 1 ms periodic interrupt that 
 *    services and increments a custom hardware time structure.
 * 3. Sleep & Time: Written from scratch using the hardware timer ISR.
 * ============================================================================
 */

#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>

/* Number of task slots in the scheduling frame */
#define SLOTX   4
/* Maximum number of sub-cycles/tasks within a single slot */
#define CYCLEX  5
/* Slot duration in milliseconds (Williams uses 5000 ms = 5 s) */
#define SLOT_T  5000 

/**
 * Hardware Time Structure
 * Incremented by the hardware timer ISR
 */
typedef struct {
    volatile uint32_t millis;
    volatile uint32_t seconds;
} HardwareTime_t;

static HardwareTime_t g_system_time = {0, 0};

/**
 * Hardware Timer Hook / ISR Callback
 * In the SAMD21 Arduino core, SysTick triggers at 1 kHz (1 ms period).
 * We maintain our custom hardware time structure here.
 */
extern "C" void osSystickHandler(void) {
    g_system_time.millis++;
    if ((g_system_time.millis % 1000) == 0) {
        g_system_time.seconds++;
    }
}

/**
 * Returns current timestamp in milliseconds from the ISR time structure
 */
uint32_t get_system_time_ms(void) {
    // Fallback to millis() if osSystickHandler hook is overridden by RTOS
    return (g_system_time.millis > 0) ? g_system_time.millis : millis();
}

/**
 * Custom sleep function: replaces POSIX sleep(seconds)
 * Pauses execution for the requested duration in seconds using the timer ISR.
 */
void custom_sleep(uint32_t seconds) {
    uint32_t target_ms = get_system_time_ms() + (seconds * 1000);
    while (get_system_time_ms() < target_ms) {
        // Yield/wait for the hardware timer ISR to increment the time structure
        yield();
    }
}

/* Cyclic Executive Execution State */
static int cycle = 0;
static int slot  = 0;
static uint32_t now  = 0;
static uint32_t then = 0;

/* --- Task Definitions (from Williams Section 4.7) --- */

void one(void) {
    Serial.println("task 1 running");
    custom_sleep(1);
}

void two(void) {
    Serial.println("task 2 running");
    custom_sleep(2);
}

void three(void) {
    Serial.println("task 3 running");
    custom_sleep(3);
}

void four(void) {
    Serial.println("task 4 running");
    custom_sleep(4);
}

void five(void) {
    Serial.println("task 5 running");
    custom_sleep(5);
}

/**
 * Burn Task: Fills remaining slot time until next slot tick
 */
void burn(void) {
    uint32_t bstart = get_system_time_ms();
    
    // Busy-wait burn until the 5000 ms slot boundary is reached
    while ((now = get_system_time_ms()) - then < SLOT_T) {
        /* Burn CPU time here until slot period completes */
        yield();
    }
    
    uint32_t elapsed_burn = get_system_time_ms() - bstart;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "burn time = %lu ms\r\n", (unsigned long)elapsed_burn);
    Serial.println(buffer);
    
    then = now;
    cycle = CYCLEX; // Force move to next slot in dispatcher
}

/* Task Table: Function pointers arranged in 4 slots with 5 entries each */
typedef void (*TaskFunction_t)(void);

static TaskFunction_t ttable[SLOTX][CYCLEX] = {
    { one,  two,   burn, burn, burn },
    { one,  three, burn, burn, burn },
    { one,  four,  burn, burn, burn },
    { burn, burn,  burn, burn, burn }
};

#ifndef UNIT_TEST
void setup(void) {
    // Initialize USB Serial for MKR Zero
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait up to 3 seconds for Serial Monitor to connect
    }
    
    Serial.println("==============================================");
    Serial.println(" Williams Ch 4.7 Demonstration Cyclic Executive");
    Serial.println(" Target: Arduino MKR Zero (ATSAMD21G18A)");
    Serial.println(" Hardware Timer: 1000 Hz SysTick ISR");
    Serial.println(" Slot Period: 5000 ms");
    Serial.println("==============================================\r\n");
    
    then = get_system_time_ms();
}

void loop(void) {
    // Dispatcher loop matching Williams Ch 4.7
    for (slot = 0; slot < SLOTX; slot++) {
        char slot_hdr[32];
        snprintf(slot_hdr, sizeof(slot_hdr), ">>> Starting Slot %d <<<", slot);
        Serial.println(slot_hdr);
        
        for (cycle = 0; cycle < CYCLEX; cycle++) {
            (*ttable[slot][cycle])(); // Dispatch next task from table
        }
    }
}
#endif
