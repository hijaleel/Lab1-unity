#include <sam.h>
#include <stdio.h>
#include "calc.h"

int main(void)
{
    /* Initialize SAMD21 System Clock & Bus */
    SystemInit();

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

    /* volatile variables prevent the compiler from optimizing calculations away */
    volatile int test_sum = sum_res;
    volatile int test_pow = pow_res;
    volatile int test_mag = mag_res;

    (void)test_sum;
    (void)test_pow;
    (void)test_mag;

    while (1) {
        /* Cyclic executive / bare metal main loop */
    }
}

void setup()
{
}

void loop()
{
}
