/*
 * SmartMatrix Library - Hardware-Specific Header File (for SmartMatrix Shield V4)
 *
 * Copyright (c) 2015 Louis Beaudoin (Pixelmatix)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 // Note: only one MatrixHardware_*.h file should be included per project

#ifndef MATRIX_HARDWARE_H
#define MATRIX_HARDWARE_H
// Clockspeeds are rounded to the nearest 80Mhz / n, n >= 2, e.g. 40MHz, 26.6MHz, 20MHz, 16MHz, 13.3MHz...
#define ESP32_I2S_CLOCK_SPEED (20000000UL)

#define ESP32_FORUM_PINOUT              0
#define ESP32_FORUM_PINOUT_WITH_LATCH   1
#define SMARTLED_SHIELD_V0_PINOUT       2
#define ESP32_D1R32_PINOUT		3

//#define GPIOPINOUT ESP32_FORUM_PINOUT
//#define GPIOPINOUT ESP32_FORUM_PINOUT_WITH_LATCH // note this mode is untested as of 2018-05-17 - not being used anymore now that SmartMatrix Shield is available
//#define GPIOPINOUT SMARTLED_SHIELD_V0_PINOUT
#define GPIOPINOUT ESP32_D1R32_PINOUT

#if (GPIOPINOUT == ESP32_FORUM_PINOUT)
    // ADDX is output directly using GPIO
    #define CLKS_DURING_LATCH   0 
    #define MATRIX_I2S_MODE I2S_PARALLEL_BITS_16
    #define MATRIX_DATA_STORAGE_TYPE uint16_t

    //Upper half RGB
    #define BIT_R1  (1<<0)   
    #define BIT_G1  (1<<1)   
    #define BIT_B1  (1<<2)   
    //Lower half RGB
    #define BIT_R2  (1<<3)   
    #define BIT_G2  (1<<4)   
    #define BIT_B2  (1<<5)   
    
    // Control Signals
    #define BIT_LAT (1<<6) 
    #define BIT_OE  (1<<7)  

    #define BIT_A (1<<8)    
    #define BIT_B (1<<9)    
    #define BIT_C (1<<10)   
    #define BIT_D (1<<11)   
    #define BIT_E (1<<12)   

    #define R1_PIN  2
    #define G1_PIN  15
    #define B1_PIN  4
    #define R2_PIN  16
    #define G2_PIN  27
    #define B2_PIN  17

    #define A_PIN   5
    #define B_PIN   18
    #define C_PIN   19
    #define D_PIN   21
    #define E_PIN   12
    #define LAT_PIN 26
    #define OE_PIN  25

    #define CLK_PIN 22

    #define GPIO_PWM0A_OUT GPIO_NUM_32
    #define GPIO_SYNC0_IN  GPIO_NUM_34
#endif

#if (GPIOPINOUT == ESP32_FORUM_PINOUT_WITH_LATCH)
    // ADDX is output on RGB pins and stored in external latch (need multiple of 32-bits for full data struct, so pad 2 CLKs to 4 here)
    #define MATRIX_I2S_MODE I2S_PARALLEL_BITS_8
    #define MATRIX_DATA_STORAGE_TYPE uint8_t
    #define CLKS_DURING_LATCH   4

    //Upper half RGB
    #define BIT_R1  (1<<0)   
    #define BIT_G1  (1<<1)   
    #define BIT_B1  (1<<2)   
    //Lower half RGB
    #define BIT_R2  (1<<3)   
    #define BIT_G2  (1<<4)   
    #define BIT_B2  (1<<5)   
    
    // Control Signals
    #define BIT_LAT (1<<6) 
    #define BIT_OE  (1<<7)  

    #define BIT_A (1<<8)    
    #define BIT_B (1<<9)    
    #define BIT_C (1<<10)   
    #define BIT_D (1<<11)   
    #define BIT_E (1<<12)   

    #define R1_PIN  2
    #define G1_PIN  15
    #define B1_PIN  4
    #define R2_PIN  16
    #define G2_PIN  27
    #define B2_PIN  17

    #define A_PIN   5
    #define B_PIN   18
    #define C_PIN   19
    #define D_PIN   21
    #define E_PIN   12
    #define LAT_PIN 26
    #define OE_PIN  25

    #define CLK_PIN 22

    #define GPIO_PWM0A_OUT GPIO_NUM_32
    #define GPIO_SYNC0_IN  GPIO_NUM_34
#endif

#if (GPIOPINOUT == SMARTLED_SHIELD_V0_PINOUT)
    // ADDX is output on RGB pins and stored in external latch (need multiple of 32-bits for full data struct, so pad 2 CLKs to 4 here)
    #define MATRIX_I2S_MODE I2S_PARALLEL_BITS_8
    #define MATRIX_DATA_STORAGE_TYPE uint8_t
    #define CLKS_DURING_LATCH   4

    //Upper half RGB
    #define BIT_R1 (1<<0)  
    #define BIT_G1 (1<<1)  
    #define BIT_B1 (1<<2)  
    //Lower half RGB
    #define BIT_R2 (1<<3)  
    #define BIT_G2 (1<<4)  
    #define BIT_B2 (1<<5)  

    // Control Signals
    #define BIT_LAT (1<<6)
    #define BIT_OE (1<<7) 

    #define R1_PIN  4
    #define G1_PIN  21
    #define B1_PIN  22
    #define R2_PIN  2
    #define G2_PIN  25
    #define B2_PIN  0

    #define A_PIN   -1
    #define B_PIN   -1
    #define C_PIN   -1
    #define D_PIN   -1
    #define E_PIN   -1

    #define LAT_PIN 27
    #define OE_PIN  32

    #define CLK_PIN 26

    #define GPIO_PWM0A_OUT GPIO_NUM_33
    #define GPIO_SYNC0_IN  GPIO_NUM_32
#endif

#if (GPIOPINOUT == ESP32_D1R32_PINOUT)
    // ADDX is output directly using GPIO
    #define CLKS_DURING_LATCH   0
    #define MATRIX_I2S_MODE I2S_PARALLEL_BITS_16
    #define MATRIX_DATA_STORAGE_TYPE uint16_t

    //Upper half RGB
    #define BIT_R1  (1<<0)
    #define BIT_G1  (1<<1)
    #define BIT_B1  (1<<2)
    //Lower half RGB
    #define BIT_R2  (1<<3)
    #define BIT_G2  (1<<4)
    #define BIT_B2  (1<<5)
  
    // Control Signals
    #define BIT_LAT (1<<6)
    #define BIT_OE  (1<<7)

    #define BIT_A (1<<8)
    #define BIT_B (1<<9)
    #define BIT_C (1<<10)
    #define BIT_D (1<<11)
    #define BIT_E (1<<12)

    #define R1_PIN  18
    #define G1_PIN  19
    #define B1_PIN  23
    #define R2_PIN  5
    #define G2_PIN  13
    #define B2_PIN  12

    #define A_PIN   14
    #define B_PIN   27
    #define C_PIN   16
    #define D_PIN   17
    #define E_PIN   0
    #define LAT_PIN 26
    #define OE_PIN  2

    #define CLK_PIN 25

    #define GPIO_PWM0A_OUT GPIO_NUM_32
    #define GPIO_SYNC0_IN  GPIO_NUM_34
#endif


//#define DEBUG_PINS_ENABLED
#define DEBUG_1_GPIO    GPIO_NUM_13
//#define DEBUG_2_GPIO    GPIO_NUM_12

#endif
