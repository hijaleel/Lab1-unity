/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Data Acquisition System, Noise & Jitter Analysis (Questions 9 - 12)
 * 
 * Student Name: Jaleel Gregory
 * ID NO:        816039976
 * Platform:     Arduino MKR Zero (Microchip ATSAMD21G18A ARM Cortex-M0+)
 * Compiler:     arm-none-eabi-gcc (GNU ARM Embedded Toolchain)
 * ============================================================================
 * Description:
 * 1. 1000 Hz ADC Sampling with busy-wait synchronization.
 * 2. Collects 1000 samples into DataBuf[1000] and timestamps into TimeBuf[1000].
 * 3. Calculates Sampling Jitter: maxT, minT, Jitter = maxT - minT (in microseconds).
 * 4. Pure integer statistics (no floating point):
 *    - Signal = Mean (integer division)
 *    - Noise  = Standard Deviation (integer square root)
 *    - SNR    = Signal / Noise
 *    - ENOB   = log2(SNR)
 * 5. Probability Mass Function (PMF) histogram generation.
 * 6. Configures SAMD21 ADC Hardware Averaging via ADC->AVGCTRL.
 * ============================================================================
 */

#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>

#define NUM_SAMPLES 1000
#define ADC_PIN     A1 // MKR Zero ADC input pin (PB02 / A1)

/* Buffers required by specification */
static uint16_t DataBuf[NUM_SAMPLES];
static uint32_t TimeBuf[NUM_SAMPLES];

/* Probability Mass Function (PMF) frequency bins: 10-bit ADC has 1024 codes */
static uint16_t PMF[1024];

/**
 * Fast integer square root algorithm (Newton-Raphson / bitwise)
 * Avoids any floating point unit usage as mandated.
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
 * Integer base-2 logarithm (floor of log2)
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
 * Configure ATSAMD21 ADC Hardware Averaging
 * avg_samples: 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
 */
void set_samd21_hardware_averaging(uint16_t avg_samples) {
    uint8_t samplenum = 0;
    uint8_t adjust = 0;

    switch (avg_samples) {
        case 1:    samplenum = ADC_AVGCTRL_SAMPLENUM_1_Val;    adjust = 0; break;
        case 2:    samplenum = ADC_AVGCTRL_SAMPLENUM_2_Val;    adjust = 1; break;
        case 4:    samplenum = ADC_AVGCTRL_SAMPLENUM_4_Val;    adjust = 2; break;
        case 8:    samplenum = ADC_AVGCTRL_SAMPLENUM_8_Val;    adjust = 3; break;
        case 16:   samplenum = ADC_AVGCTRL_SAMPLENUM_16_Val;   adjust = 4; break;
        case 32:   samplenum = ADC_AVGCTRL_SAMPLENUM_32_Val;   adjust = 4; break;
        case 64:   samplenum = ADC_AVGCTRL_SAMPLENUM_64_Val;   adjust = 4; break;
        case 128:  samplenum = ADC_AVGCTRL_SAMPLENUM_128_Val;  adjust = 4; break;
        case 256:  samplenum = ADC_AVGCTRL_SAMPLENUM_256_Val;  adjust = 4; break;
        case 512:  samplenum = ADC_AVGCTRL_SAMPLENUM_512_Val;  adjust = 4; break;
        case 1024: samplenum = ADC_AVGCTRL_SAMPLENUM_1024_Val; adjust = 4; break;
        default:   samplenum = ADC_AVGCTRL_SAMPLENUM_1_Val;    adjust = 0; break;
    }

    // Wait for synchronization before writing
    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM(samplenum) | ADC_AVGCTRL_ADJRES(adjust);
    while (ADC->STATUS.bit.SYNCBUSY);
}

/**
 * Perform single ADC acquisition with busy-wait synchronization
 */
uint16_t adc_read_busy_wait(void) {
    return (uint16_t)analogRead(ADC_PIN);
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

    set_samd21_hardware_averaging(hw_avg);

    // Warm-up reading
    analogRead(ADC_PIN);
    delay(10);

    /* --- 1. 1000 Hz Sampling Loop with Busy-Wait Synchronization --- */
    uint32_t next_sample_time = micros();
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Busy wait until periodic 1000 Hz (1000 microseconds) boundary arrives
        while ((int32_t)(micros() - next_sample_time) < 0) {
            __NOP();
        }
        TimeBuf[i] = micros();
        DataBuf[i] = adc_read_busy_wait();
        next_sample_time += 1000; // Target 1000 us = 1 ms interval
    }

    /* --- 2. Calculate Sampling Jitter --- */
    uint32_t maxT = 0;
    uint32_t minT = 0xFFFFFFFF;

    for (int i = 1; i < NUM_SAMPLES; i++) {
        uint32_t dt = TimeBuf[i] - TimeBuf[i - 1];
        if (dt > maxT) maxT = dt;
        if (dt < minT) minT = dt;
    }
    uint32_t jitter = maxT - minT;

    /* --- 3. Calculate Signal (Mean) and Noise (Std Dev) (Integer Only) --- */
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

    // To prevent division by zero in perfectly noiseless ideal simulations
    if (noise_stddev == 0) noise_stddev = 1;

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

    analogReadResolution(10); // Standard 10-bit resolution

    Serial.println("\r\n=========================================================");
    Serial.println(" ECNG3006 Lab 1: Data Acquisition System & Jitter/Noise");
    Serial.println(" Student: Jaleel Gregory | ID: 816039976");
    Serial.println(" Sampling Rate: 1000 Hz | Busy-Wait Synchronized");
    Serial.println("=========================================================");

    // Perform an initial scan across two hardware averaging configurations:
    // 1. No hardware averaging (1 sample)
    // 2. Hardware averaging (16 samples)
    run_data_acquisition(1);
    delay(2000);
    run_data_acquisition(16);
}

void loop(void) {
    // Keep board alive; user can trigger another capture by typing 'r' or entering text
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
