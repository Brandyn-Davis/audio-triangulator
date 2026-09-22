#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "clock.h"
#include "uart0.h"
#include "nvic.h"
#include "timer.h"
#include "dma.h"
#include "adc0.h"
#include "adc1.h"

#define RED_LED PORTF,1
#define AIN0 PORTE,3
#define AIN1 PORTE,2
#define AIN2 PORTE,1
#define AIN4 PORTD,3

#define STR_LEN 60
#define SAMPLE_LEN 4
#define SSFIFO1_MAX_LEN 4
#define XCORR_LEN BUFFERED_ITEMS*2-1
#define AVERAGING_SAMPLES 64                //
//#define TRIGGER_ABOVE_AVG 1000               // Triggers occur when sound is louder than this value + average noise
#define TRIGGER_RECALC_TIME 2               // Seconds to recalculate and set trigger based on average noise

#define MAX_SOUND_CYCLE_DIFF 8886           // Maximum number of cycles sound will take to reach the farthest microphone (0.0762 m * (1/343 m/s)) * 40000000 cycles

uint16_t COOLDOWN_TIME = 3;
uint16_t TRIGGER_ABOVE_AVG = 700;

bool showFails = false;
bool showData = false;
bool showTdoa = false;
bool keepComparatorInterruptsOff = false;
uint16_t mic1Averaged = 0;
uint16_t mic2Averaged = 0;
uint16_t mic3Averaged = 0;

uint16_t primaryBuffer[BUFFERED_ITEMS] = {0};
uint16_t alternateBuffer[BUFFERED_ITEMS] = {0};

uint16_t buffer1[BUFFERED_ITEMS] = {0};
uint16_t buffer2[BUFFERED_ITEMS] = {0};
uint16_t buffer3[BUFFERED_ITEMS] = {0};

bool primaryCompleted;                   // Identifies whether the primary or alternate buffer was last [0: Alternate, 1: Primary]

void initHw()
{
    initSystemClockTo40Mhz();

    // Enable ports for analog input
    enablePort(PORTE);
    enablePort(PORTD);
    _delay_cycles(3);

    // Enable ports for I/O
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure red LED
    selectPinPushPullOutput(RED_LED);
    setPinValue(RED_LED, 0);

    // Set pins for analog input
    selectPinAnalogInput(AIN0);
    selectPinAnalogInput(AIN1);
    selectPinAnalogInput(AIN2);
    selectPinAnalogInput(AIN4);
}

void dmaIsr()
{
    //stopDma();
    primaryCompleted = !primaryCompleted;
    ADC0_ISC_R |= ADC_ISC_IN1;
    ADC0_ISC_R |= ADC_ISC_DCINSS1;
}

void cooldownCallback()
{
    keepComparatorInterruptsOff = false;
    //ADC1_DCCTL0_R |= ADC_DCCTL0_CIE;
    stopTimer(cooldownCallback);
    deleteTimer(cooldownCallback);
}

void ledTimerCallback()
{
    setPinValue(RED_LED, 0);
    stopTimer(ledTimerCallback);
    deleteTimer(ledTimerCallback);
}

// Returns # of samples B lags A
int16_t getOffset(uint16_t* A, uint16_t* B)
{
    int16_t xcorrOffset;
    uint32_t sum;
    uint32_t max = 0;
    uint16_t maxIndex = 0;
    uint16_t i, j;
    //char str[20];

    for (i = 0; i < XCORR_LEN; i++)
    {
        sum = 0;
        xcorrOffset = i - (BUFFERED_ITEMS-1);

        // Cross-correlate
        for (j = 0; j < BUFFERED_ITEMS; j++)
        {
            if (j-xcorrOffset >= 0 && j-xcorrOffset < BUFFERED_ITEMS)   // in-range check
            {
                sum += B[j] * A[j-xcorrOffset];
                //sprintf(str, "%d ", sum);
                //putsUart0(str);
            }
        }
        //putsUart0("\n");

        // Update max if new one found
        if (sum > max)
        {
            max = sum;
            maxIndex = i;
        }
    }

    return (maxIndex + 1) - BUFFERED_ITEMS;     // Return index of max normalized to center
}

void comparatorIsr()
{
    char str[STR_LEN];
    //uint16_t buffer1[BUFFERED_ITEMS] = {0};
    //uint16_t buffer2[BUFFERED_ITEMS] = {0};
    //uint16_t buffer3[BUFFERED_ITEMS] = {0};
    float interpolFactor;
    uint16_t* buffer = (primaryCompleted) ? primaryBuffer : alternateBuffer;

    // Asynchronous LED blink
    setPinValue(RED_LED, 1);
    startOneshotTimer(ledTimerCallback, 2);

    // Cooldown period
    ADC1_DCCTL0_R &= ~ADC_DCCTL0_CIE;                           // Block comparator interrupts
    keepComparatorInterruptsOff = true;                         // Don't trigger interrupts until cooldown ends
    startOneshotTimer(cooldownCallback, COOLDOWN_TIME);

    // Wait T/2
    _delay_cycles(MAX_SOUND_CYCLE_DIFF/2);
    //_delay_cycles(MAX_SOUND_CYCLE_DIFF/8);

    // Stop DMA
    stopDma();
    //_delay_cycles(1000);

    // Interpolate upsampled mic buffers
    int i;
    for (i = 0; i < BUFFERED_ITEMS-4; i++)
    {
        interpolFactor = (buffer[i+4]-buffer[i])/4.0;

        switch (i%4)
        {
        case 0:
            buffer1[i]   = buffer[i];
            buffer1[i+1] = buffer[i] + round(1*interpolFactor);
            buffer1[i+2] = buffer[i] + round(2*interpolFactor);
            buffer1[i+3] = buffer[i] + round(3*interpolFactor);
            break;
        case 1:
            buffer2[i-1] = buffer[i];
            buffer2[i]   = buffer[i] + round(1*interpolFactor);
            buffer2[i+1] = buffer[i] + round(2*interpolFactor);
            buffer2[i+2] = buffer[i] + round(3*interpolFactor);
            break;
        case 2:
            buffer3[i-2] = buffer[i];
            buffer3[i-1] = buffer[i] + round(1*interpolFactor);
            buffer3[i]   = buffer[i] + round(2*interpolFactor);
            buffer3[i+1] = buffer[i] + round(3*interpolFactor);
            break;
        }
    }
    buffer1[BUFFERED_ITEMS-4] = buffer1[BUFFERED_ITEMS-3] = buffer1[BUFFERED_ITEMS-2] = buffer1[BUFFERED_ITEMS-1] = buffer[BUFFERED_ITEMS-2];
    buffer2[BUFFERED_ITEMS-4] = buffer2[BUFFERED_ITEMS-3] = buffer2[BUFFERED_ITEMS-2] = buffer2[BUFFERED_ITEMS-1] = buffer[BUFFERED_ITEMS-3];
    buffer3[BUFFERED_ITEMS-4] = buffer3[BUFFERED_ITEMS-3] = buffer3[BUFFERED_ITEMS-2] = buffer3[BUFFERED_ITEMS-1] = buffer[BUFFERED_ITEMS-4];

    if (showData)
    {
        // Print to see interpolated values
        putsUart0("Mic 1:\n");
        for (i = 0; i < BUFFERED_ITEMS-1; i++)
        {
            sprintf(str, "%d,", buffer1[i]);
            putsUart0(str);
        }
        sprintf(str, "%d\n", buffer1[i]);
        putsUart0(str);

        putsUart0("Mic 2:\n");
        for (i = 0; i < BUFFERED_ITEMS-1; i++)
        {
            sprintf(str, "%d,", buffer2[i]);
            putsUart0(str);
        }
        sprintf(str, "%d\n", buffer2[i]);
        putsUart0(str);

        putsUart0("Mic 3:\n");
        for (i = 0; i < BUFFERED_ITEMS-1; i++)
        {
            sprintf(str, "%d,", buffer3[i]);
            putsUart0(str);
        }
        sprintf(str, "%d\n", buffer3[i]);
        putsUart0(str);
    }

    // Do cross-correlations to get mic offsets in samples
    int16_t mic12 = getOffset(buffer1, buffer2);
    int16_t mic13 = getOffset(buffer1, buffer3);
    int16_t mic23 = getOffset(buffer2, buffer3);

    if (showTdoa)
    {
        // Print mic offsets
        sprintf(str, "mic12: %d, mic13: %d, mic23: %d\n", mic12, mic13, mic23);
        putsUart0(str);

        if (mic12 > 0)
        {
            sprintf(str, "mic2 is %d samples after mic1\n", abs(mic12));
            putsUart0(str);
        }
        else if (mic12 == 0)
        {
            putsUart0("mic1 and mic2 are matched in time\n");
        }
        else
        {
            sprintf(str, "mic1 is %d samples after mic2\n", abs(mic12));
            putsUart0(str);
        }

        if (mic13 > 0)
        {
            sprintf(str, "mic3 is %d samples after mic1\n", abs(mic13));
            putsUart0(str);
        }
        else if (mic13 == 0)
        {
            putsUart0("mic1 and mic3 are matched in time\n");
        }
        else
        {
            sprintf(str, "mic1 is %d samples after mic3\n", abs(mic13));
            putsUart0(str);
        }

        if (mic23 > 0)
        {
            sprintf(str, "mic3 is %d samples after mic2\n", abs(mic23));
            putsUart0(str);
        }
        else if (mic23 == 0)
        {
            putsUart0("mic2 and mic3 are matched in time\n");
        }
        else
        {
            sprintf(str, "mic2 is %d samples after mic3\n", abs(mic23));
            putsUart0(str);
        }
    }

    // Identify which mic received the signal first
    uint8_t firstMic = 1;
    if (mic12 >= 0 && mic13 >= 0)
        firstMic = 1;
    else if (-1*mic12 >= 0 && mic23 >= 0)
        firstMic = 2;
    else if (-1*mic13 >= 0 && -1*mic23 >= 0)
        firstMic = 3;

    //sprintf(str, "first mic: %d\n", firstMic);
    //putsUart0(str);

    // Calculate angle
    // facing mic 1: 0 deg
    // facing mic 2: 120 deg
    // facing mic 3: 240 deg
    float k1 = 2;
    float k2 = 0.8;
    float angle;
    switch(firstMic)
    {
        case 1:
            angle = k1*mic23 + k2*mic23*mic23;
            break;

        case 2:
            angle = 120 + k1*(-1*mic13) + k2*(-1*mic13)*(-1*mic13);
            break;

        case 3:
            angle = 240 + k1*mic12 + k2*mic12;
            break;
    }
    angle += 60;

    if (((angle < 0 || angle > 360) && showFails) || (angle >= 0 && angle <= 360))
    {
        // Print angle
        sprintf(str, "angle: %f\n\n", angle);
        putsUart0(str);
    }

    restartDma();

    // Clear comparator interrupt
    ADC1_DCISC_R |= ADC_DCISC_DCINT0;
}

void callbackAvgCalculation()
{
    uint32_t sum1 = 0;
    uint32_t sum2 = 0;
    uint32_t sum3 = 0;
    int i;

    // Stop DMA
    stopDma();
    _delay_cycles(1000);

    // Calculate average of mic1
    for (i = 0; i < AVERAGING_SAMPLES; i++)
    {
        switch (i%4)
        {
        case 0:
            sum1 += primaryBuffer[i];
            break;
        case 1:
            sum2 += primaryBuffer[i];
            break;
        case 2:
            sum3 += primaryBuffer[i];
            break;
        }
    }
    mic1Averaged = sum1/(AVERAGING_SAMPLES/4);
    mic2Averaged = sum2/(AVERAGING_SAMPLES/4);
    mic3Averaged = sum3/(AVERAGING_SAMPLES/4);

    // Use the new value for triggering (mic1)
    updateTrigger(mic1Averaged + TRIGGER_ABOVE_AVG);

    restartDma();
}

#define MAX_CHARS 80
char strInput[MAX_CHARS+1];
char* token;
uint8_t count = 0;
uint8_t n = 0;
void userInterface()
{
    char str[STR_LEN];
    char c;
    bool end;

    if (kbhitUart0())
    {
        c = getcUart0();

        end = (c == 13) || (count == MAX_CHARS);
        if (!end)
        {
            if ((c == 8 || c == 127) && count > 0)
                count--;
            if (c >= ' ' && c < 127)
                strInput[count++] = c;
        }
        else
        {
            strInput[count] = '\0';
            count = 0;
            token = strtok(strInput, " ");
            if (strcmp(token, "average") == 0)
            {
                sprintf(str, "mic1 avg: %d\nmic2 avg: %d\nmic3 avg: %d\n", mic1Averaged, mic2Averaged, mic3Averaged);
                putsUart0(str);
            }
            else if (strcmp(token, "aoa") == 0)
            {
                comparatorIsr();
            }
            else if (strcmp(token, "tdoa") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "on") == 0)
                {
                    showTdoa = true;
                }
                else if (strcmp(token, "off") == 0)
                {
                    showTdoa = false;
                }
            }
            else if (strcmp(token, "data") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "on") == 0)
                {
                    showData = true;
                }
                else if (strcmp(token, "off") == 0)
                {
                    showData = false;
                }
            }
            else if (strcmp(token, "level") == 0)
            {
                token = strtok(NULL, " ");
                sscanf(token, "%hhu", &n);

                TRIGGER_ABOVE_AVG = n;
            }
            else if (strcmp(token, "fail") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "on") == 0)
                {
                    showFails = true;
                }
                else if (strcmp(token, "off") == 0)
                {
                    showFails = false;
                }
            }
            else if (strcmp(token, "holdoff") == 0)
            {
                token = strtok(NULL, " ");
                sscanf(token, "%hhu", &n);

                COOLDOWN_TIME = n;
            }
        }
    }
}

int main(void)
{
    initHw();
    initAdc0();
    initAdc1();
    initTimer();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    enableNvicInterrupt(INT_ADC1SS1);   // NVIC <- ADC1 SS1 (for comparator interrupts)
    enableNvicInterrupt(INT_ADC0SS1);   // NVIC <- ADC0 SS1 (for DMA interrupts)
    initDma(&ADC0_SSFIFO1_R, primaryBuffer, alternateBuffer);

    // Normalize trigger to average noise periodically
    startPeriodicTimer(callbackAvgCalculation, TRIGGER_RECALC_TIME);

    while (1) {

        if (keepComparatorInterruptsOff)
            ADC1_DCCTL0_R &= ~ADC_DCCTL0_CIE;
        else
            ADC1_DCCTL0_R |= ADC_DCCTL0_CIE;

        userInterface();


        _delay_cycles(10000);
    }
}
