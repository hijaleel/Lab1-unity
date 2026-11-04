/**
 * ============================================================================
 * Course: ECNG3006 - Microprocessor Systems
 * Lab 1: Questions 9 - 12 Backup - ADC Data Acquisition System & Jitter/Noise
 * 
 * Student Name: Jaleel Gregory
 * ID NO:        816039976
 * ============================================================================
 */

#include <Arduino.h>
#include <sam.h>
#include <stdint.h>
#include <stdio.h>

#define NUM_SAMPLES 1000

static uint16_t DataBuf[NUM_SAMPLES];
static uint32_t TimeBuf[NUM_SAMPLES];
static uint16_t PMF[1024];

void samd21_adc_init_baremetal(void) {
    PM->APBCMASK.reg |= PM_APBCMASK_ADC;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_ADC_Val) |
                        GCLK_CLKCTRL_GEN(GCLK_CLKCTRL_GEN_GCLK0_Val) |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    PORT->Group[1].PINCFG[2].bit.PMUXEN = 1;
    PORT->Group[1].PMUX[1].bit.PMUXE = PORT_PMUX_PMUXE_B_Val;

    ADC->CTRLA.bit.SWRST = 1;
    while (ADC->STATUS.bit.SYNCBUSY || ADC->CTRLA.bit.SWRST);

    ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL_INTVCC1;
    ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV128 | ADC_CTRLB_RESSEL_10BIT;
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS_PIN10 | ADC_INPUTCTRL_MUXNEG_GND;
    ADC->CTRLA.bit.ENABLE = 1;
    while (ADC->STATUS.bit.SYNCBUSY);
}

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

    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM(samplenum) | ADC_AVGCTRL_ADJRES(adjust);
    while (ADC->STATUS.bit.SYNCBUSY);
}

uint16_t samd21_adc_read_baremetal(void) {
    while (ADC->STATUS.bit.SYNCBUSY);
    ADC->SWTRIG.bit.START = 1;
    while (!(ADC->INTFLAG.bit.RESRDY));
    return (uint16_t)ADC->RESULT.reg;
}

