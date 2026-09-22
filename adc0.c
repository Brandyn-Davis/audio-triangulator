#include <stdint.h>
#include "adc0.h"
#include "tm4c123gh6pm.h"

enum analogInputNumbers {AIN0_NUM, AIN1_NUM, AIN2_NUM, AIN3_NUM, AIN4_NUM};

void initAdc0()
{
    SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_R0;                          // Enable uDMA clock for ADC0
    _delay_cycles(20);

    configureAdc0Ss1();
}

void configureAdc0Ss1()
{
    ADC0_ACTSS_R &= ~ADC_ACTSS_ASEN1;                               // Disable ADC0 SS1 before configuration

    ADC0_SSMUX1_R |= AIN1_NUM << ADC_SSMUX1_MUX0_S;                 // Associate AIN1 pin to 1st sequence sample
    ADC0_SSMUX1_R |= AIN2_NUM << ADC_SSMUX1_MUX1_S;                 // Associate AIN2 pin to 2nd sequence sample
    ADC0_SSMUX1_R |= AIN4_NUM << ADC_SSMUX1_MUX2_S;                 // Associate AIN4 pin to 3rd sequence sample
    ADC0_SSMUX1_R |= AIN0_NUM << ADC_SSMUX1_MUX3_S;                 // (unused but set because DMA arbitration size needs to be powers of 2)
    ADC0_SSCTL1_R |= ADC_SSCTL1_END3 | ADC_SSCTL1_IE3;              // Set 4th sample to be end point and generate interrupts(for DMA)

    ADC0_EMUX_R |= ADC_EMUX_EM1_ALWAYS;                             // Configure ADC0 SS1 to continuously sample
    //ADC0_IM_R |= ADC_IM_MASK1;                                      // Pass ADC0 SS1 raw interrupts to interrupt controller

    ADC0_ACTSS_R |= ADC_ACTSS_ASEN1;                                // Enable ADC0 SS1
}

