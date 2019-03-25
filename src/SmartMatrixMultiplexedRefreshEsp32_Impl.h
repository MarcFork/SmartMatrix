/*
 * SmartMatrix Library - Refresh Code for Teensy 3.x Platform
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

#include "SmartMatrix3.h"

#if 0
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_heap_caps.h"
#include "i2s_parallel.h"
#endif

#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "rom/lldesc.h"

static void setupTimer(void) {
    // invert OE-PWM output
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    gpio_matrix_out(GPIO_PWM0A_OUT, PWM0_OUT0A_IDX, true, false);

    // setup GPIO for sync from OE-DMA (invert signal)
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_SYNC_0, GPIO_SYNC0_IN);
    gpio_matrix_in(GPIO_SYNC0_IN, PWM0_SYNC0_IN_IDX, true);
    gpio_pulldown_en(GPIO_SYNC0_IN);   //Enable pull down on SYNC0 signal

    // can't use the library function directly as library has hardcoded large prescale values
    //mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);   //Configure PWM0A & PWM0B with above settings

    mcpwm_unit_t mcpwm_num = MCPWM_UNIT_0;
    mcpwm_timer_t timer_num = MCPWM_TIMER_0;

    //esp_err_t mcpwm_init(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num, const mcpwm_config_t *mcpwm_conf)
    {

        #define MATRIX_MCPWM_CLK_PRESCL 0       //MCPWM clock prescale 
        #define MATRIX_TIMER_CLK_PRESCALE 0      //MCPWM timer prescales
        #define MATRIX_MCPWM_BASE_CLK (2 * APB_CLK_FREQ)   //2*APB_CLK_FREQ 160Mhz
        #define MATRIX_MCPWM_CLK (MATRIX_MCPWM_BASE_CLK/(MATRIX_MCPWM_CLK_PRESCL +1))

        mcpwm_dev_t *MCPWM[2] = {&MCPWM0, &MCPWM1};

        periph_module_enable((periph_module_t)(PERIPH_PWM0_MODULE + mcpwm_num));
        MCPWM[mcpwm_num]->clk_cfg.prescale = MATRIX_MCPWM_CLK_PRESCL;

        // set period to max, we never intend for it to wrap, compa to 4 ticks (~25 ns, around the lowest visible OE pulse)
        MCPWM[mcpwm_num]->timer[timer_num].period.prescale = MATRIX_TIMER_CLK_PRESCALE;
        MCPWM[mcpwm_num]->timer[timer_num].period.period = 0xFFFF;
        MCPWM[mcpwm_num]->timer[timer_num].period.upmethod = 0;
        MCPWM[mcpwm_num]->channel[timer_num].cmpr_value[0].cmpr_val = 4;
        MCPWM[mcpwm_num]->channel[timer_num].cmpr_cfg.a_upmethod = 0;

        MCPWM[mcpwm_num]->timer[timer_num].mode.mode = MCPWM_UP_COUNTER;
        mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);

        mcpwm_start(mcpwm_num, timer_num);
        MCPWM[mcpwm_num]->timer_sel.operator0_sel = 0;
        MCPWM[mcpwm_num]->timer_sel.operator1_sel = 1;
        MCPWM[mcpwm_num]->timer_sel.operator2_sel = 2;
        MCPWM[mcpwm_num]->update_cfg.global_up_en = 1;
        MCPWM[mcpwm_num]->update_cfg.global_force_up = 1;
        MCPWM[mcpwm_num]->update_cfg.global_force_up = 0;
    }

    //6. Syncronization configuration
    mcpwm_sync_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_SELECT_SYNC0, 0);    //Load counter value with 20% of period counter of mcpwm timer 1 when sync 0 occurs
}



#define INLINE __attribute__( ( always_inline ) ) inline


// TODO: slower refresh rates require larger timer values - get the min refresh rate from the largest MSB value that will fit in the timer (round up)
#define MIN_REFRESH_RATE    30

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void frameShiftCompleteISR(void);    

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
CircularBuffer SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::dmaBuffer;

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
uint16_t SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::refreshRate = 120;

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
uint16_t SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::minRefreshRate = 60;

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
uint8_t SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::lsbMsbTransitionBit = 0;

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
typename SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::frameStruct * SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::matrixUpdateFrames[ESP32_NUM_FRAME_BUFFERS];

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::SmartMatrix3RefreshMultiplexed(void) {
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
bool SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::isFrameBufferFree(void) {
    if(cbIsFull(&dmaBuffer))
        return false;
    else
        return true;
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
typename SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::frameStruct * SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::getNextFrameBufferPtr(void) {
    return matrixUpdateFrames[cbGetNextWrite(&dmaBuffer)];
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::writeFrameBuffer(uint8_t currentFrame) {
    //SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::frameStruct * currentFramePtr = SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::getNextFrameBufferPtr();
    i2s_parallel_flip_to_buffer(&I2S1, cbGetNextWrite(&dmaBuffer));
    cbWrite(&dmaBuffer);
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::recoverFromDmaUnderrun(void) {

}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
typename SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::matrix_calc_callback SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::matrixCalcCallback;

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::setMatrixCalculationsCallback(matrix_calc_callback f) {
    setShiftCompleteCallback(f);
    matrixCalcCallback = f;
}

// large factor = more dim, default is full brightness
template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
int SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::dimmingFactor = dimmingMaximum - (100 * 255)/100;

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::setBrightness(uint8_t newBrightness) {
    dimmingFactor = dimmingMaximum - newBrightness;
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::setRefreshRate(uint16_t newRefreshRate) {
    if(newRefreshRate > MIN_REFRESH_RATE)
        minRefreshRate = newRefreshRate;
    else
        minRefreshRate = MIN_REFRESH_RATE;
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
uint16_t SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::getRefreshRate(void) {
    return refreshRate;
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::begin(uint32_t dmaRamToKeepFreeBytes) {
    cbInit(&dmaBuffer, ESP32_NUM_FRAME_BUFFERS);

    printf("Starting SmartMatrix DMA Mallocs\r\n");

    // TODO: malloc this buffer before other smaller buffers as this is (by far) the largest buffer to allocate?
    matrixUpdateFrames[0] = (frameStruct *)heap_caps_malloc(sizeof(frameStruct), MALLOC_CAP_DMA);
    assert(matrixUpdateFrames[0] != NULL);
    matrixUpdateFrames[1] = (frameStruct *)heap_caps_malloc(sizeof(frameStruct), MALLOC_CAP_DMA);
    assert(matrixUpdateFrames[1] != NULL);

    printf("sizeof framestruct: %08X\r\n", (uint32_t)sizeof(frameStruct));
    printf("matrixUpdateFrames[0] pointer: %08X\r\n", (uint32_t)matrixUpdateFrames[0]);
    printf("matrixUpdateFrames[1] pointer: %08X\r\n", (uint32_t)matrixUpdateFrames[1]);

    printf("Frame Structs Allocated from Heap:\r\n");
    printf("Heap Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    printf("8-bit Accessible Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("32-bit Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
    printf("DMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    printf("Allocating refresh buffer:\r\nDMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    // setup debug output
#ifdef DEBUG_PINS_ENABLED
    gpio_pad_select_gpio(DEBUG_1_GPIO);
    gpio_set_direction(DEBUG_1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DEBUG_1_GPIO, 1);
    gpio_set_level(DEBUG_1_GPIO, 0);
#endif

    setupTimer();

    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory
    int numDescriptorsPerRow;
    lsbMsbTransitionBit = 0;
    int ramrequired = 0;
#ifndef SCAN_BY_ROW	// SCAN_BY_BITPLANE
    int bitplaneBytes = sizeof(rowBitStruct) * MATRIX_SCAN_MOD;
    int numDescriptorsPerBitplane = (bitplaneBytes + DMA_MAX-1)/DMA_MAX;
    printf( "Bitplanes take %d bytes, requiring %d DMA descriptors each.\r\n", bitplaneBytes, numDescriptorsPerBitplane );
#endif

    while(1) {
#ifdef SCAN_BY_ROW
        numDescriptorsPerRow = 1;
        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
            numDescriptorsPerRow += 1<<(i - lsbMsbTransitionBit - 1);
        }
        ramrequired = numDescriptorsPerRow * MATRIX_SCAN_MOD * ESP32_NUM_FRAME_BUFFERS * sizeof(lldesc_t);
#else // SCAN_BY_BITPLANE
        ramrequired = ((1 << (COLOR_DEPTH_BITS-lsbMsbTransitionBit)) + lsbMsbTransitionBit - 1) * numDescriptorsPerBitplane * ESP32_NUM_FRAME_BUFFERS * sizeof(lldesc_t);
#endif
        int largestblockfree = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

        printf("lsbMsbTransitionBit of %d requires %d RAM, %d available, leaving %d free: \r\n", lsbMsbTransitionBit, ramrequired, largestblockfree, largestblockfree - ramrequired);

        if(largestblockfree > dmaRamToKeepFreeBytes && ramrequired < (largestblockfree - dmaRamToKeepFreeBytes))
            break;

        if(lsbMsbTransitionBit < COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    if(ramrequired > heap_caps_get_largest_free_block(MALLOC_CAP_DMA)){
        printf("not enough RAM for SmartMatrix descriptors\r\n");
        return;
    }

    printf("Raised lsbMsbTransitionBit to %d/%d to fit in RAM\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1);

    // calculate the lowest LSBMSB_TRANSITION_BIT value that will fit in memory that will meet or exceed the configured refresh rate
    while(1) {
        int psPerClock = 1000000000000UL/ESP32_I2S_CLOCK_SPEED;
        int nsPerLatch = ((PIXELS_PER_LATCH + CLKS_DURING_LATCH) * psPerClock) / 1000;
        //printf("ns per latch: %d: \r\n", nsPerLatch);        

        // add time to shift out LSBs + LSB-MSB transition bit - this ignores fractions...
        int nsPerRow = COLOR_DEPTH_BITS * nsPerLatch;

        // add time to shift out MSBs
        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++)
            nsPerRow += (1<<(i - lsbMsbTransitionBit - 1)) * (COLOR_DEPTH_BITS - i) * nsPerLatch;

        //printf("nsPerRow: %d: \r\n", nsPerRow);        

        int nsPerFrame = nsPerRow * MATRIX_SCAN_MOD;
        //printf("nsPerFrame: %d: \r\n", nsPerFrame);        

        int actualRefreshRate = 1000000000UL/(nsPerFrame);

        refreshRate = actualRefreshRate;

        printf("lsbMsbTransitionBit of %d gives %d Hz refresh, %d requested: \r\n", lsbMsbTransitionBit, actualRefreshRate, minRefreshRate);        

        if(actualRefreshRate >= minRefreshRate)
            break;

        if(lsbMsbTransitionBit < COLOR_DEPTH_BITS - 1)
            lsbMsbTransitionBit++;
        else
            break;
    }

    printf("Raised lsbMsbTransitionBit to %d/%d to meet minimum refresh rate\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1);

    // TODO: completely fill buffer with data before enabling DMA - can't do this now, lsbMsbTransition bit isn't set in the calc class - also this call will probably have no effect as matrixCalcDivider will skip the first call
    //matrixCalcCallback();

    // lsbMsbTransition Bit is now finalized - redo descriptor count in case it changed to hit min refresh rate
#ifdef SCAN_BY_ROW
    numDescriptorsPerRow = 1;
    for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
        numDescriptorsPerRow += 1<<(i - lsbMsbTransitionBit - 1);
    }

    int desccount = numDescriptorsPerRow * MATRIX_SCAN_MOD;
#else // SCAN_BY_BITPLANE
    int desccount = ((1 << (COLOR_DEPTH_BITS-lsbMsbTransitionBit)) + lsbMsbTransitionBit - 1) * numDescriptorsPerBitplane;
#endif
    printf("Descriptors for lsbMsbTransitionBit %d/%d with %d rows require %d bytes of DMA RAM\r\n", lsbMsbTransitionBit, COLOR_DEPTH_BITS - 1, MATRIX_SCAN_MOD, ESP32_NUM_FRAME_BUFFERS * desccount * sizeof(lldesc_t));

    // malloc the DMA linked list descriptors that i2s_parallel will need
    lldesc_t * dmadesc_a = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if(!dmadesc_a) {
        printf("can't malloc dmadesc_a");
        return;
    }
    lldesc_t * dmadesc_b = (lldesc_t *)heap_caps_malloc(desccount * sizeof(lldesc_t), MALLOC_CAP_DMA);
    if(!dmadesc_b) {
        printf("can't malloc dmadesc_b");
        return;
    }

    printf("SmartMatrix Mallocs Complete\r\n");
    printf("Heap Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
    printf("8-bit Accessible Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("32-bit Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
    printf("DMA Memory Available: %d bytes total, %d bytes largest free block: \r\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    lldesc_t *prevdmadesca = 0;
    lldesc_t *prevdmadescb = 0;
#ifdef SCAN_BY_ROW 
    // fill DMA linked lists for both frames
    int currentDescOffset = 0;
    for(int j=0; j<MATRIX_SCAN_MOD; j++) {
        // first set of data is LSB through MSB, single pass - all color bits are displayed once, which takes care of everything below and inlcluding LSBMSB_TRANSITION_BIT
        // TODO: size must be less than DMA_MAX - worst case for SmartMatrix Library: 16-bpp with 256 pixels per row would exceed this, need to break into two
        link_dma_desc(&dmadesc_a[currentDescOffset], prevdmadesca, matrixUpdateFrames[0]->rowdata[j].rowbits[0].data, sizeof(rowBitStruct) * COLOR_DEPTH_BITS);
        prevdmadesca = &dmadesc_a[currentDescOffset];
        link_dma_desc(&dmadesc_b[currentDescOffset], prevdmadescb, matrixUpdateFrames[1]->rowdata[j].rowbits[0].data, sizeof(rowBitStruct) * COLOR_DEPTH_BITS);
        prevdmadescb = &dmadesc_b[currentDescOffset];
        currentDescOffset++;
        //printf("row %d: \r\n", j);

        for(int i=lsbMsbTransitionBit + 1; i<COLOR_DEPTH_BITS; i++) {
            // binary time division setup: we need 2 of bit (LSBMSB_TRANSITION_BIT + 1) four of (LSBMSB_TRANSITION_BIT + 2), etc
            // because we sweep through to MSB each time, it divides the number of times we have to sweep in half (saving linked list RAM)
            // we need 2^(i - LSBMSB_TRANSITION_BIT - 1) == 1 << (i - LSBMSB_TRANSITION_BIT - 1) passes from i to MSB
            //printf("buffer %d: repeat %d times, size: %d, from %d - %d\r\n", nextBufdescIndex, 1<<(i - LSBMSB_TRANSITION_BIT - 1), (COLOR_DEPTH_BITS - i), i, COLOR_DEPTH_BITS-1);
            for(int k=0; k < 1<<(i - lsbMsbTransitionBit - 1); k++) {
                link_dma_desc(&dmadesc_a[currentDescOffset], prevdmadesca, matrixUpdateFrames[0]->rowdata[j].rowbits[i].data, sizeof(rowBitStruct) * (COLOR_DEPTH_BITS - i));
                prevdmadesca = &dmadesc_a[currentDescOffset];
                link_dma_desc(&dmadesc_b[currentDescOffset], prevdmadescb, matrixUpdateFrames[1]->rowdata[j].rowbits[i].data, sizeof(rowBitStruct) * (COLOR_DEPTH_BITS - i));
                prevdmadescb = &dmadesc_b[currentDescOffset];

                currentDescOffset++;
                //printf("i %d, j %d, k %d\r\n", i, j, k);
            }
        }
    }
    //End markers
    dmadesc_a[desccount-1].eof = 1;
    dmadesc_b[desccount-1].eof = 1;
    dmadesc_a[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_a[0];
    dmadesc_b[desccount-1].qe.stqe_next=(lldesc_t*)&dmadesc_b[0];

#else // SCAN_BY_BITPLANE
// temporal dithering of bitplanes to reduce flicker and maximize accuracy of image captured by partial frame camera shutter speeds.
    lldesc_t *currdmadesc_a = dmadesc_a;
    lldesc_t *currdmadesc_b = dmadesc_b;
    for(int k=1; k < 1<<(COLOR_DEPTH_BITS - lsbMsbTransitionBit - 1); k++) {
	int i = COLOR_DEPTH_BITS-ffs(k);
        link_large_dma( &currdmadesc_a, &prevdmadesca, matrixUpdateFrames[0]->bitplane[i].rowbit[0].data, bitplaneBytes );
        link_large_dma( &currdmadesc_b, &prevdmadescb, matrixUpdateFrames[1]->bitplane[i].rowbit[0].data, bitplaneBytes );
//        printf("i %d, k %d, size %d\r\n", i, k, bitplaneBytes );
    }
// bitplanes <= lsbMsbTransitionBit are inserted in the middle where the lsb would naturally sit in the dithering sequence.
// They need to be output sequentially because encoded partial brightness straddles bitplanes.
    for(int i=0; i<=lsbMsbTransitionBit; i++) {
        link_large_dma( &currdmadesc_a, &prevdmadesca, matrixUpdateFrames[0]->bitplane[i].rowbit[0].data, bitplaneBytes );
        link_large_dma( &currdmadesc_b, &prevdmadescb, matrixUpdateFrames[1]->bitplane[i].rowbit[0].data, bitplaneBytes );
    }
    for(int k=1; k < 1<<(COLOR_DEPTH_BITS - lsbMsbTransitionBit - 1); k++) {
	int i = COLOR_DEPTH_BITS-ffs(k);
        link_large_dma( &currdmadesc_a, &prevdmadesca, matrixUpdateFrames[0]->bitplane[i].rowbit[0].data, bitplaneBytes );
        link_large_dma( &currdmadesc_b, &prevdmadescb, matrixUpdateFrames[1]->bitplane[i].rowbit[0].data, bitplaneBytes );
//        printf("i %d, k %d, size %d\r\n", i, k, bitplaneBytes );
    }
    //End markers
    prevdmadesca->eof = prevdmadescb->eof = 1;
    prevdmadesca->qe.stqe_next = dmadesc_a;
    prevdmadescb->qe.stqe_next = dmadesc_b;
#endif


    //printf("\n");

    i2s_parallel_config_t cfg={
        .gpio_bus={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, LAT_PIN, OE_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, -1, -1, -1},
        .gpio_clk=CLK_PIN,
        .clkspeed_hz=ESP32_I2S_CLOCK_SPEED,  // formula used is 80000000L/(cfg->clkspeed_hz + 1), must result in >=2.  Acceptable values 26.67MHz, 20MHz, 16MHz, 13.34MHz...
        .bits=MATRIX_I2S_MODE,
        .bufa=0,
        .bufb=0,
        desccount,
        desccount,
        dmadesc_a,
        dmadesc_b
    };

    //Setup I2S
    i2s_parallel_setup_without_malloc(&I2S1, &cfg);

    //printf("I2S setup done.\n");
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
void SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::markRefreshComplete(void) {
    if(!cbIsEmpty(&SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::dmaBuffer))
        cbRead(&SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::dmaBuffer);
}

template <int refreshDepth, int matrixWidth, int matrixHeight, unsigned char panelType, unsigned char optionFlags>
uint8_t SmartMatrix3RefreshMultiplexed<refreshDepth, matrixWidth, matrixHeight, panelType, optionFlags>::getLsbMsbTransitionBit(void) {
    return lsbMsbTransitionBit;
}
