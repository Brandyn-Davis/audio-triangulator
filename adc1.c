#include <stdint.h>
#include <stdbool.h>
#include "adc1.h"
//#include "gpio.h"
//#include "timer.h"
#include "tm4c123gh6pm.h"

#define RED_LED PORTF,1
#define DIG_CMP_VAL 400
enum analogInputNumbers {AIN0_NUM, AIN1_NUM, AIN2_NUM, AIN3_NUM, AIN4_NUM};

//bool keepComparatorInterruptsOff = false;

void initAdc1()
{
    // Enable clock for analog comparator 0
    SYSCTL_RCGCACMP_R = SYSCTL_RCGCACMP_R0;
    _delay_cycles(3);

    // Enable clock for ADC1
    SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_R1;
    _delay_cycles(20);

    configureAdc1Ss1();
}

void configureAdc1Ss1()
{
    ADC1_ACTSS_R &= ~ADC_ACTSS_ASEN1;                           // Disable ADC1 SS1 for configuration

    ADC1_SSMUX1_R |= AIN1_NUM << ADC_SSMUX1_MUX0_S;             // Associate AIN1 pin to 1st sequence sample
    ADC1_SSCTL1_R |= ADC_SSCTL1_END0;                           // Set 1st sample to be end point

    ADC1_SSOP1_R |= ADC_SSOP1_S0DCOP;                           // Configure 1st sample to be sent to a comparator
    ADC1_SSDC1_R &= ~ADC_SSDC1_S0DCSEL_M;
    ADC1_SSDC1_R |= CMP0 << ADC_SSDC1_S0DCSEL_S;                // Configure 1st sample to be sent to comparator 0

    ADC1_EMUX_R |= ADC_EMUX_EM1_ALWAYS;                         // Continuously run ADC1 SS1
    //ADC1_EMUX_R |= ADC_EMUX_EM1_PROCESSOR;

    // Configure digital comparator 0
    ADC1_DCCTL0_R |= ADC_DCCTL0_CIM_ALWAYS;                     // Set comparison interrupt mode to Always
    ADC1_DCCTL0_R |= ADC_DCCTL0_CIC_HIGH;                       // Set interrupt on high band condition
    ADC1_DCCMP0_R |= DIG_CMP_VAL << ADC_DCCMP0_COMP0_S;         // Set COMP0(lo-band)
    ADC1_DCCMP0_R |= DIG_CMP_VAL << ADC_DCCMP0_COMP1_S;         // Set COMP1(hi-band)

    ADC1_IM_R |= ADC_IM_DCONSS1;

    ADC1_ACTSS_R |= ADC_ACTSS_ASEN1;                           // Enable ADC1 SS1
}

void updateTrigger(uint16_t triggerVal)
{
    ADC1_ACTSS_R &= ~ADC_ACTSS_ASEN1;                           // Disable ADC1 SS1 for configuration

    ADC1_DCCMP0_R |= DIG_CMP_VAL << ADC_DCCMP0_COMP0_S;         // Set COMP0(lo-band)
    ADC1_DCCMP0_R |= DIG_CMP_VAL << ADC_DCCMP0_COMP1_S;         // Set COMP1(hi-band)

    ADC1_ACTSS_R |= ADC_ACTSS_ASEN1;                           // Enable ADC1 SS1
}

/*void cooldownCallback()
{
    keepComparatorInterruptsOff = false;
    stopTimer(cooldownCallback);
    deleteTimer(cooldownCallback);
}

void ledTimerCallback()
{
    setPinValue(RED_LED, 0);
    stopTimer(ledTimerCallback);
    deleteTimer(ledTimerCallback);
}

void comparatorIsr()
{
    // Asynchronous LED blink
    setPinValue(RED_LED, 1);
    startOneshotTimer(ledTimerCallback, 2);

    // Cooldown period
    ADC1_DCCTL0_R &= ~ADC_DCCTL0_CIE;                           // Block comparator interrupts
    keepComparatorInterruptsOff = true;                         // Don't trigger interrupts until cooldown ends
    startOneshotTimer(cooldownCallback, 3);

    // Clear comparator interrupt
    ADC1_DCISC_R |= ADC_DCISC_DCINT0;
}*/

/*void adc1AveragingConfig()
{
    // Disable ADC1 SS1 before configuration
    ADC1_ACTSS_R &= ~ADC_ACTSS_ASEN1;

    // Configure HW averaging & stop sending to comparator
    ADC1_SAC_R = 6;     // 6 -> 64x HW averaging
    ADC1_SSOP1_R &= ~ADC_SSOP1_S0DCOP;

    // Enable ADC1 SS1
    ADC1_ACTSS_R |= ADC_ACTSS_ASEN1;
}*/
