#ifndef ADC1_H_
#define ADC1_H_

enum Comparators {CMP0, CMP1, CMP2, CMP3, CMP4, CMP5, CMP6, CMP7};

void initAdc1();
void configureAdc1Ss1();
void updateTrigger(uint16_t triggerVal);

//void cooldownCallback();
//void ledTimerCallback();
//void comparatorIsr();

#endif
