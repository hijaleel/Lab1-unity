#ifndef UNIT_TEST

#include <sam.h>
#include "calc.h"

/*
 * ECNG3006 - Bare Metal SAMD21 UART Driver using sam.h
 * Target: Arduino MKR Zero (ATSAMD21G18A)
 * 
 * Hardware Mapping for MKR Zero TX/RX pins (Pins 14 & 13 on headers):
 * - PB22 = SERCOM5 Pad 2 (TX) -> Peripheral Alt function D
 * - PB23 = SERCOM5 Pad 3 (RX) -> Peripheral Alt function D
 */

static void uart_init_samd21(uint32_t baudrate)
{
    /* 1. Enable APBC clock for SERCOM5 */
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;

    /* 2. Route Generic Clock Generator 0 (Main 48MHz clock) to SERCOM5 core */
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM5_GCLK_ID_CORE) |
                        GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    /* 3. Configure Port B pins PB22 (TX) and PB23 (RX) for Peripheral D (SERCOM-ALT) */
    PORT->Group[1].PINCFG[22].bit.PMUXEN = 1;
    PORT->Group[1].PINCFG[23].bit.PMUXEN = 1;

    /* Peripheral Function D = 0x3 (SERCOM-ALT for SERCOM5 on PB22/PB23) */
    PORT->Group[1].PMUX[11].reg = PORT_PMUX_PMUXE_D | PORT_PMUX_PMUXO_D;

    /* 4. Reset SERCOM5 */
    SERCOM5->USART.CTRLA.bit.SWRST = 1;
    while (SERCOM5->USART.CTRLA.bit.SWRST || SERCOM5->USART.SYNCBUSY.bit.SWRST);

    /* 5. Configure USART Mode, Pinout, and Data Order */
    SERCOM5->USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
                               SERCOM_USART_CTRLA_DORD |
                               SERCOM_USART_CTRLA_RXPO(3) |
                               SERCOM_USART_CTRLA_TXPO(1) |
                               SERCOM_USART_CTRLA_SAMPR(0);

    /* 6. Calculate and set BAUD register (assuming 48 MHz GCLK0) */
    uint64_t baud_val = 65536ULL * (48000000ULL - 16ULL * baudrate) / 48000000ULL;
    SERCOM5->USART.BAUD.reg = (uint16_t)baud_val;

    /* 7. Enable Transmitter and Receiver in CTRLB */
    SERCOM5->USART.CTRLB.reg = SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_RXEN;
    while (SERCOM5->USART.SYNCBUSY.bit.CTRLB);

    /* 8. Enable SERCOM5 */
    SERCOM5->USART.CTRLA.bit.ENABLE = 1;
    while (SERCOM5->USART.SYNCBUSY.bit.ENABLE);
}

static void uart_putc(char c)
{
    while (!(SERCOM5->USART.INTFLAG.bit.DRE));
    SERCOM5->USART.DATA.reg = (uint16_t)c;
}

static void uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
        }
        uart_putc(*str++);
    }
}

static void uart_put_int(int val)
{
    char buf[16];
    int i = 0;
    if (val == 0) {
        uart_putc('0');
        return;
    }
    if (val < 0) {
        uart_putc('-');
        val = -val;
    }
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

void setup(void)
{
    /* Initialize bare-metal UART on SERCOM5 at 115200 baud without Arduino Serial */
    uart_init_samd21(115200);

    /* 
     * [QUESTION 2 REQUIREMENT - 2 MARKS]
     * Main program demonstrating the new magnitude function
     * alongside existing calc functions.
     */
    int val_a = 15;
    int val_b = -250;
    int p_base = 2;
    int p_exp = 8;

    int sum_res = add(val_a, 25);
    int pow_res = power(p_base, p_exp);
    int mag_res = magnitude(val_b);

    uart_puts("\n=========================================\n");
    uart_puts(" Microprocessor Lab 1 - Execution Models \n");
    uart_puts(" Bare-metal sam.h UART (No Arduino.h)    \n");
    uart_puts("=========================================\n\n");

    uart_puts("add(15, 25)      = ");
    uart_put_int(sum_res);
    uart_puts("\n");

    uart_puts("power(2, 8)      = ");
    uart_put_int(pow_res);
    uart_puts("\n");

    uart_puts("magnitude(-250)  = ");
    uart_put_int(mag_res);
    uart_puts("\n");
}

void loop(void)
{
    /* Cyclic executive / bare metal main loop */
}

#endif /* UNIT_TEST */
