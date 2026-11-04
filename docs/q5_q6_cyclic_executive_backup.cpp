/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Questions 5 & 6 - Demonstration Cyclic Executive Code
 * 
 * Student Name: Jaleel Gregory
 * ID NO : 816039976
 * Platform:     Arduino MKR Zero (Microchip ATSAMD21G18A ARM Cortex-M0+)
 * Compiler:     arm-none-eabi-gcc (GNU ARM Embedded Toolchain)
 * ============================================================================
 * Preserved standalone reference file for Questions 5 and 6.
 * ============================================================================
 */

#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>

#define SLOTX   4
#define CYCLEX  5
#define SLOT_T  5000 

typedef struct {
    volatile uint32_t millis;
    volatile uint32_t seconds;
} HardwareTime_t;

static HardwareTime_t g_system_time = {0, 0};

extern "C" void osSystickHandler(void) {
    g_system_time.millis++;
    if ((g_system_time.millis % 1000) == 0) {
        g_system_time.seconds++;
    }
}

uint32_t get_system_time_ms(void) {
    return (g_system_time.millis > 0) ? g_system_time.millis : millis();
}

void custom_sleep(uint32_t seconds) {
    uint32_t target_ms = get_system_time_ms() + (seconds * 1000);
    while (get_system_time_ms() < target_ms) {
        yield();
    }
}

static int cycle = 0;
static int slot  = 0;
static uint32_t now  = 0;
static uint32_t then = 0;

void one(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%lu ms] task 1 running", (unsigned long)get_system_time_ms());
    Serial.println(buf);
    custom_sleep(1);
}

void two(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%lu ms] task 2 running", (unsigned long)get_system_time_ms());
    Serial.println(buf);
    custom_sleep(2);
}

void three(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%lu ms] task 3 running", (unsigned long)get_system_time_ms());
    Serial.println(buf);
    custom_sleep(3);
}

void four(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%lu ms] task 4 running", (unsigned long)get_system_time_ms());
    Serial.println(buf);
    custom_sleep(4);
}

void five(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "[%lu ms] task 5 running", (unsigned long)get_system_time_ms());
    Serial.println(buf);
    custom_sleep(5);
}

void burn(void) {
    uint32_t bstart = get_system_time_ms();
    while ((now = get_system_time_ms()) - then < SLOT_T) {
        yield();
    }
    uint32_t elapsed_burn = get_system_time_ms() - bstart;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "burn time = %lu ms\r\n", (unsigned long)elapsed_burn);
    Serial.println(buffer);
    then = now;
    cycle = CYCLEX;
}

typedef void (*TaskFunction_t)(void);

static TaskFunction_t ttable[SLOTX][CYCLEX] = {
    { one,  two,   burn, burn, burn },
    { one,  three, burn, burn, burn },
    { one,  four,  burn, burn, burn },
    { burn, burn,  burn, burn, burn }
};
