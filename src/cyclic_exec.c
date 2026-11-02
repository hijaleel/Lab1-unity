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
 * Standalone C implementation of Rob Williams Chapter 4, Section 4.7
 * "Demonstration Cyclic Executive Code" amended for ATSAMD21 bare-metal.
 * ============================================================================
 */

#include <stdint.h>
#include <stdio.h>
#include <sam.h>

#define SLOTX   4
#define CYCLEX  5
#define SLOT_T  5000 /* 5000 ms = 5 sec slot time */

/* Time structure incremented by hardware timer ISR */
typedef struct {
    volatile uint32_t millis;
    volatile uint32_t seconds;
} HardwareTime_t;

static HardwareTime_t g_time = {0, 0};

/* Hardware SysTick ISR: fires every 1 ms */
void SysTick_Handler(void) {
    g_time.millis++;
    if ((g_time.millis % 1000) == 0) {
        g_time.seconds++;
    }
}

/* Time function */
uint32_t time_ms(void) {
    return g_time.millis;
}

/* Sleep function based on hardware timer ISR */
void sleep_sec(uint32_t sec) {
    uint32_t target = time_ms() + (sec * 1000);
    while (time_ms() < target) {
        __NOP();
    }
}

static int cycle = 0;
static int slot  = 0;
static uint32_t now  = 0;
static uint32_t then = 0;

/* Tasks from Williams Ch 4.7 */
void one(void) {
    /* Print message and sleep 1s */
    sleep_sec(1);
}

void two(void) {
    /* Print message and sleep 2s */
    sleep_sec(2);
}

void three(void) {
    /* Print message and sleep 3s */
    sleep_sec(3);
}

void four(void) {
    /* Print message and sleep 4s */
    sleep_sec(4);
}

void five(void) {
    /* Print message and sleep 5s */
    sleep_sec(5);
}

void burn(void) {
    uint32_t bstart = time_ms();
    while ((now = time_ms()) - then < SLOT_T) {
        /* Burn CPU time here until slot period completes */
        __NOP();
    }
    then = now;
    cycle = CYCLEX;
}

typedef void (*task_fn)(void);

static task_fn ttable[SLOTX][CYCLEX] = {
    { one,  two,   burn, burn, burn },
    { one,  three, burn, burn, burn },
    { one,  four,  burn, burn, burn },
    { burn, burn,  burn, burn, burn }
};
