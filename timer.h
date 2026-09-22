#ifndef TIMER_H_
#define TIMER_H_

typedef void (*_callback)();

void initTimer();
bool startOneshotTimer(_callback callback, uint32_t seconds);
bool startPeriodicTimer(_callback callback, uint32_t seconds);
bool stopTimer(_callback callback);
bool restartTimer(_callback callback);
bool deleteTimer(_callback callback);
void tickIsr();
uint32_t random32();

#endif
