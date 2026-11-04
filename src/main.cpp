/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Data Acquisition System, Noise & Jitter Analysis (Questions 9 - 12)
 * 
 * Student Name: Jaleel Gregory
 * ID :        816039976
 * Platform:     Arduino MKR Zero (Microchip ATSAMD21G18A ARM Cortex-M0+)
 * Compiler:     arm-none-eabi-gcc (GNU ARM Embedded Toolchain)
 * ============================================================================
 * Description:
 * Bare-metal SAMD21 (sam.h / CMSIS) implementation of 1000 Hz ADC DAQ :
 * 1. Configures ADC via sam.h registers (GCLK, PM, ADC->CTRLA, ADC->AVGCTRL).
 * 2. Samples AIN[10] / A1 (PB02) at 1000 Hz using busy-wait synchronization.
 * 3. Collects 1000 samples into DataBuf[1000] and timestamps into TimeBuf[1000].
 * 4. Calculates Sampling Jitter: maxT, minT, Jitter = maxT - minT (in microseconds).
 * 5. Pure integer statistics (no floating point):
 *    - Signal = Mean (integer division)
 *    - Noise  = Standard Deviation (integer square root)
 *    - SNR    = Signal / Noise
 *    - ENOB   = log2(SNR)
 * 6. Probability Mass Function (PMF) histogram generation.
 * ============================================================================
 */

#include <Arduino.h> // Provides standard board init & USB CDC Serial interface
#include <sam.h>     // Bare-metal SAMD21 direct register definitions
#include <stdint.h>
#include <stdio.h>

#define NUM_SAMPLES 1000

/* Buffers required by specification */
static uint16_t DataBuf[NUM_SAMPLES];
static uint32_t TimeBuf[NUM_SAMPLES];

/* Probability Mass Function (PMF) frequency bins: 10-bit ADC has 1024 codes */
static uint16_t PMF[1024];

/**
 * Configure ADC directly via bare-metal sam.h registers
 * Channel: AIN[10] on PB02 (which maps to pin A1 on MKR Zero)
 */
void samd21_adc_init_baremetal(void) {
    // 1. Enable peripheral clock for ADC in Power Manager (PM)
    PM->APBCMASK.reg |= PM_APBCMASK_ADC;

    // 2. Route Generic Clock Generator 0 (48 MHz) to GCLK_ADC
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_ADC_Val) |
                        GCLK_CLKCTRL_GEN(GCLK_CLKCTRL_GEN_GCLK0_Val) |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    // 3. Configure Pin PB02 (AIN[10]) for peripheral function B (Analog Input)
    PORT->Group[1].PINCFG[2].bit.PMUXEN = 1;
    PORT->Group[1].PMUX[1].bit.PMUXE = PORT_PMUX_PMUXE_B_Val; // Even pin PB02 = B (ADC AIN10)

    // 4. Reset ADC
    ADC->CTRLA.bit.SWRST = 1;
    while (ADC->STATUS.bit.SYNCBUSY || ADC->CTRLA.bit.SWRST);

    // 5. Configure ADC Reference (Internal VDDANA / 1.65V or 1/2 VDDANA)
    // Use VDDANA (3.3V) as standard reference: REFSEL = 1/2 VDDANA or INTREF
    ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL_INTVCC1; // 1/2 VDDANA (equivalent to 1.65V) or 3.3V
    
    // 6. Set prescaler to DIV128 (48 MHz / 128 = 375 kHz ADC clock, well within 30-2100 kHz spec)
    ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV128 | ADC_CTRLB_RESSEL_10BIT;

    // 7. Select input: AIN[10] (PB02) on Positive MUX, GND on Negative MUX
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS_PIN10 | ADC_INPUTCTRL_MUXNEG_GND;

    // 8. Enable ADC
    ADC->CTRLA.bit.ENABLE = 1;
    while (ADC->STATUS.bit.SYNCBUSY);
}

/**
 * Set SAMD21 ADC Hardware Averaging directly via sam.h register AVGCTRL
 */
void samd21_adc_set_hardware_averaging_baremetal(uint16_t avg_samples) {
    uint8_t samplenum = 0;
    uint8_t adjust = 0;

    switch (avg_samples) {
        case 1:    samplenum = ADC_AVGCTRL_SAMPLENUM_1_Val;    adjust = 0; break;
        case 2:    samplenum = ADC_AVGCTRL_SAMPLENUM_2_Val;    adjust = 1; break;
        case 4:    samplenum = ADC_AVGCTRL_SAMPLENUM_4_Val;    adjust = 2; break;
        case 8:    samplenum = ADC_AVGCTRL_SAMPLENUM_8_Val;    adjust = 3; break;
        case 16:   samplenum = ADC_AVGCTRL_SAMPLENUM_16_Val;   adjust = 4; break;
        default:   samplenum = ADC_AVGCTRL_SAMPLENUM_1_Val;    adjust = 0; break;
    }

    // Wait for synchronization before updating register
    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM(samplenum) | ADC_AVGCTRL_ADJRES(adjust);
    while (ADC->STATUS.bit.SYNCBUSY);
}

/**
 * Bare-metal ADC conversion with busy-wait synchronization
 */
uint16_t samd21_adc_read_baremetal(void) {
    // Start conversion
    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->SWTRIG.bit.START = 1;

    // Busy-wait synchronization until conversion is complete (RESRDY flag)
    while (!(ADC->INTFLAG.bit.RESRDY));

    // Clear flag and return 10-bit result
    return (uint16_t)ADC->RESULT.reg;
}

/**
 * Integer square root algorithm (no floating point)
 */
uint32_t isqrt(uint64_t n) {
    if (n == 0) return 0;
    uint64_t x0 = n / 2;
    if (x0 == 0) return 1;
    uint64_t x1 = (x0 + n / x0) / 2;
    while (x1 < x0) {
        x0 = x1;
        x1 = (x0 + n / x0) / 2;
    }
    return (uint32_t)x0;
}

/**
 * Integer log2 (floor of log2)
 */
uint32_t ilog2(uint32_t val) {
    if (val == 0) return 0;
    uint32_t log = 0;
    while (val >>= 1) {
        log++;
    }
    return log;
}

/**
 * Run Data Acquisition at 1000 Hz, calculate Jitter, SNR, ENOB, and output PMF
 */
void run_data_acquisition(uint16_t hw_avg) {
    char str[128];
    snprintf(str, sizeof(str), "\r\n======================================================\r\n"
                               " Starting DAQ Acquisition (Hardware Averaging = %u)\r\n"
                               "======================================================\r\n", hw_avg);
    Serial.print(str);

    samd21_adc_set_hardware_averaging_baremetal(hw_avg);

    // Warm-up conversion
    samd21_adc_read_baremetal();
    delay(10);

    /* --- 1. 1000 Hz Sampling Loop with Busy-Wait Synchronization --- */
    uint32_t next_sample_time = micros();
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Busy wait until periodic 1000 Hz (1000 us) boundary arrives
        while ((int32_t)(micros() - next_sample_time) < 0) {
            __NOP();
        }
        TimeBuf[i] = micros();
        DataBuf[i] = samd21_adc_read_baremetal(); // Read ADC via sam.h registers
        next_sample_time += 1000;                 // Target 1000 us = 1 ms interval
    }

    /* --- 2. Calculate Sampling Jitter (Question 10) --- */
    uint32_t maxT = 0;
    uint32_t minT = 0xFFFFFFFF;

    for (int i = 1; i < NUM_SAMPLES; i++) {
        uint32_t dt = TimeBuf[i] - TimeBuf[i - 1];
        if (dt > maxT) maxT = dt;
        if (dt < minT) minT = dt;
    }
    uint32_t jitter = maxT - minT;

    /* --- 3. Calculate Signal (Mean) and Noise (Std Dev) (Pure Integer) --- */
    uint64_t sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += DataBuf[i];
    }
    uint32_t mean_signal = (uint32_t)(sum / NUM_SAMPLES);

    uint64_t variance_sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int32_t diff = (int32_t)DataBuf[i] - (int32_t)mean_signal;
        variance_sum += (uint64_t)(diff * diff);
    }
    uint32_t noise_stddev = isqrt(variance_sum / NUM_SAMPLES);
    if (noise_stddev == 0) noise_stddev = 1; // Prevent div by zero

    uint32_t snr = mean_signal / noise_stddev;
    uint32_t enob = ilog2(snr);

    /* --- 4. Report Results --- */
    Serial.println("----- TIMING & JITTER RESULTS (Question 10) -----");
    snprintf(str, sizeof(str), "Samples Recorded: %d\r\n", NUM_SAMPLES); Serial.print(str);
    snprintf(str, sizeof(str), "Target Interval : 1000 us (1000 Hz)\r\n"); Serial.print(str);
    snprintf(str, sizeof(str), "maxT: %lu us\r\n", (unsigned long)maxT); Serial.print(str);
    snprintf(str, sizeof(str), "minT: %lu us\r\n", (unsigned long)minT); Serial.print(str);
    snprintf(str, sizeof(str), "Sampling Jitter (maxT - minT): %lu us\r\n", (unsigned long)jitter); Serial.print(str);

    Serial.println("\r\n----- SIGNAL & NOISE RESULTS (Questions 9 & 11) -----");
    snprintf(str, sizeof(str), "Hardware Averaging: %u sample(s)\r\n", hw_avg); Serial.print(str);
    snprintf(str, sizeof(str), "Signal (Mean ADC code): %lu (out of 1023)\r\n", (unsigned long)mean_signal); Serial.print(str);
    snprintf(str, sizeof(str), "Noise (Std Dev)       : %lu\r\n", (unsigned long)noise_stddev); Serial.print(str);
    snprintf(str, sizeof(str), "SNR (Signal / Noise)  : %lu\r\n", (unsigned long)snr); Serial.print(str);
    snprintf(str, sizeof(str), "ENOB = log2(SNR)      : %lu bits\r\n", (unsigned long)enob); Serial.print(str);

    /* --- 5. Build and Output PMF Histogram (Question 11) --- */
    memset(PMF, 0, sizeof(PMF));
    uint16_t min_observed = 1023;
    uint16_t max_observed = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint16_t code = DataBuf[i];
        if (code < 1024) {
            PMF[code]++;
            if (code < min_observed) min_observed = code;
            if (code > max_observed) max_observed = code;
        }
    }

    Serial.println("\r\n----- PROBABILITY MASS FUNCTION (PMF) TABLE -----");
    Serial.println("ADC_Code,Frequency,Probability_x1000");
    for (uint16_t c = min_observed; c <= max_observed; c++) {
        if (PMF[c] > 0) {
            snprintf(str, sizeof(str), "%u,%u,%u\r\n", c, PMF[c], (PMF[c] * 1000) / NUM_SAMPLES);
            Serial.print(str);
        }
    }
    Serial.println("----- END OF ACQUISITION -----\r\n");
}

#ifndef UNIT_TEST
void setup(void) {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait up to 3s for Serial Monitor
    }

    // Initialize ADC directly using bare-metal sam.h registers
    samd21_adc_init_baremetal();

    Serial.println("\r\n=========================================================");
    Serial.println(" ECNG3006 Lab 1: Data Acquisition System & Jitter/Noise");
    Serial.println(" Student: Jaleel Gregory | ID: 816039976");
    Serial.println(" Hardware: Microchip ATSAMD21G18A (sam.h bare-metal ADC)");
    Serial.println(" Sampling Rate: 1000 Hz | Busy-Wait Synchronized");
    Serial.println("=========================================================");

    // Run scans for No Averaging (1) and Hardware Averaging (16)
    run_data_acquisition(1);
    delay(2000);
    run_data_acquisition(16);
}

void loop(void) {
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == 'r' || ch == 'R') {
            run_data_acquisition(1);
            delay(1000);
            run_data_acquisition(16);
        }
    }
}
#endif
