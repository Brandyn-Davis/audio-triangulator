#ifndef DMA_H_
#define DMA_H_

#include <stdint.h>

//#define BUFFERED_ITEMS 128
//#define BUFFERED_ITEMS 256
#define BUFFERED_ITEMS 512
//#define BUFFERED_ITEMS 768
//#define BUFFERED_ITEMS 1024
enum XferModes {STOP, BASIC, AUTO, PINGPONG, SCATTER, ALT_SCATTER, PERIPH_SCATTER, ALT_PERIPH_SCATTER};

void initDma(volatile uint32_t* sourceAddress, uint16_t* primaryBuffer, uint16_t* alternateBuffer);
void restartDma();
void stopDma();
//void stopDmaPrimary();
//void stopDmaAlternate();
//uint16_t* getDmaPrimaryBuffer();
//uint16_t* getDmaAlternateBuffer();
void setDmaPrimaryXferMode(uint8_t mode);
void setDmaAlternateXferMode(uint8_t mode);

void enableDmaClock();
void disableDmaClock();
void enableDmaController();
void disableDmaController();
void enableDmaChannel(uint8_t channelNumber);
void disableDmaChannel(uint8_t channelNumber);
void setDmaControlBasePtr(uint8_t* controlTable);
void selectDmaChannelPeripheral(uint8_t peripheralEncoding);
void configureDmaPingPong(uint8_t channelNumber);

void debugDma(char* buffer);

#endif
