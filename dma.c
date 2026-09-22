#include <stdio.h>
#include "dma.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"

#define CH_COUNT 32
#define ADC0SS1_CH 15
#define ADC0SS1_CH_ENCODING 0
#define CTLSTRUCT_SIZE 0x10
//#define SRCEND_PTR_OFFSET 0x0
//#define DSTEND_PTR_OFFSET 0x4
//#define CTLWRD_PTR_OFFSET 0x8

/* Primary control structure control word data: */
// DSTINC   = 1 (increment storage address by 16b item size)
// DSTSIZE  = 1 (store as 16b items)
// SRCINC   = 3 (don't increment FIFO address)
// SRCSIZE  = 1 (16b items)
// ARBSIZE  = 2 (4 transfers of 16b items)
// XFERSIZE = 127 (128 items)
// XFERMODE = 3 (ping-pong)
//#define PRI_CTLWRD_DATA 0x5d0087F3
//#define PRI_CTLWRD_DATA 0x5d008ff3  // XFERSIZE = 255 (256 items)
#define PRI_CTLWRD_DATA 0x5d009ff3  // XFERSIZE = 511 (512 items)
//#define PRI_CTLWRD_DATA 0x5d00aff3  // XFERSIZE = 767 (768 items)
//#define PRI_CTLWRD_DATA 0x5d00bff3  // XFERSIZE = 1023 (1024 items)

enum ControlStructureWords {SRCEND, DSTEND, CTLWRD};

// Allocating memory for control table
#pragma DATA_ALIGN (adc0Ss1ControlTable, 1024)
uint8_t adc0Ss1ControlTable[1024];

uint32_t* primaryControlStructure;
uint32_t* alternateControlStructure;


void initDma(volatile uint32_t* sourceAddress, uint16_t* primaryBuffer, uint16_t* alternateBuffer)
{
    //char str[40] = "";
    int i;

    primaryControlStructure = (uint32_t*)(adc0Ss1ControlTable + ADC0SS1_CH * CTLSTRUCT_SIZE);
    //sprintf(str, "Primary: %p\n", primaryControlStructure);
    //putsUart0(str);
    alternateControlStructure = (uint32_t*)((uint8_t*)primaryControlStructure + 0x200);
    //sprintf(str, "Alternate: %p\n", alternateControlStructure);
    //putsUart0(str);

    enableDmaClock();
    enableDmaController();

    // Disable all channels
    for (i = 0; i < CH_COUNT; i++)
        disableDmaChannel(i);

    setDmaControlBasePtr(adc0Ss1ControlTable);
    selectDmaChannelPeripheral(ADC0SS1_CH_ENCODING);
    configureDmaPingPong(ADC0SS1_CH);

    // Set primary control structure data
    primaryControlStructure[SRCEND] = (uint32_t)sourceAddress;                           // Set source end address
    primaryControlStructure[DSTEND] = (uint32_t)(primaryBuffer + BUFFERED_ITEMS - 1);      // Set destination end address
    primaryControlStructure[CTLWRD] = PRI_CTLWRD_DATA;                                   // Set control word

    /*putsUart0("Primary:\n");
    sprintf(str, "\tSource end pointer: %p\n", (uint32_t*)primaryControlStructure[SRCEND]);
    putsUart0(str);
    sprintf(str, "\tDestination end pointer: %p\n", (uint32_t*)primaryControlStructure[DSTEND]);
    putsUart0(str);
    sprintf(str, "\tControl word: %x\n", primaryControlStructure[CTLWRD]);
    putsUart0(str);*/

    // Set alternate control structure data
    alternateControlStructure[SRCEND] = (uint32_t)sourceAddress;                         // Set source end address
    alternateControlStructure[DSTEND] = (uint32_t)(alternateBuffer + BUFFERED_ITEMS - 1);  // Set destination end address
    alternateControlStructure[CTLWRD] = PRI_CTLWRD_DATA;                                 // use same control word as primary

    /*putsUart0("Alternate:\n");
    sprintf(str, "\tSource end pointer: %p\n", (uint32_t*)alternateControlStructure[SRCEND]);
    putsUart0(str);
    sprintf(str, "\tDestination end pointer: %p\n", (uint32_t*)alternateControlStructure[DSTEND]);
    putsUart0(str);
    sprintf(str, "\tControl word: %x\n", alternateControlStructure[CTLWRD]);
    putsUart0(str);*/

    enableDmaChannel(ADC0SS1_CH);
}

void restartDma()
{
    primaryControlStructure = (uint32_t*)(adc0Ss1ControlTable + ADC0SS1_CH * CTLSTRUCT_SIZE);
    alternateControlStructure = (uint32_t*)((uint8_t*)primaryControlStructure + 0x200);

    // Reset control word to reset the XFERSIZE and MODE
    primaryControlStructure[CTLWRD] = PRI_CTLWRD_DATA;
    alternateControlStructure[CTLWRD] = PRI_CTLWRD_DATA;

    enableDmaChannel(ADC0SS1_CH);
}

void stopDma()
{
    //setDmaPrimaryXferMode(STOP);
    setDmaAlternateXferMode(STOP);
}

void setDmaPrimaryXferMode(uint8_t mode)
{
    primaryControlStructure[CTLWRD] &= ~UDMA_CHCTL_XFERMODE_M;
    primaryControlStructure[CTLWRD] |= mode & UDMA_CHCTL_XFERMODE_M;
}

void setDmaAlternateXferMode(uint8_t mode)
{
    alternateControlStructure[CTLWRD] &= ~UDMA_CHCTL_XFERMODE_M;
    alternateControlStructure[CTLWRD] |= mode & UDMA_CHCTL_XFERMODE_M;
}


void enableDmaClock()
{
    SYSCTL_RCGCDMA_R |= SYSCTL_RCGCDMA_R0;
    _delay_cycles(3);
}
void disableDmaClock()
{
    SYSCTL_RCGCDMA_R &= ~SYSCTL_RCGCDMA_R0;
    _delay_cycles(3);
}

void enableDmaController() {UDMA_CFG_R |= UDMA_CFG_MASTEN;}
void disableDmaController() {UDMA_CFG_R &= ~UDMA_CFG_MASTEN;}

void enableDmaChannel(uint8_t channelNumber) {UDMA_ENASET_R |= 1 << channelNumber;}
void disableDmaChannel(uint8_t channelNumber) {UDMA_ENACLR_R |= 1 << channelNumber;}


// Set control table's memory location
void setDmaControlBasePtr(uint8_t* controlTable)
{
    UDMA_CTLBASE_R &= ~UDMA_CTLBASE_ADDR_M;                         // Clear before writing
    UDMA_CTLBASE_R |= (uint32_t)controlTable;                       // [Don't need to shift because address is 1024 aligned]
}

// Set channel's bits to "peripheralEncoding" for selecting the peripheral
void selectDmaChannelPeripheral(uint8_t peripheralEncoding)
{
    UDMA_CHMAP1_R &= ~UDMA_CHMAP1_CH15SEL_M;                       // Clear before writing
    UDMA_CHMAP1_R |= peripheralEncoding << UDMA_CHMAP1_CH15SEL_S;
}

void configureDmaPingPong(uint8_t channelNumber)
{
    UDMA_ALTCLR_R |= 1 << channelNumber;                            // Start off with primary structure
    UDMA_USEBURSTSET_R |= 1 << channelNumber;                       // Use only bursts
    UDMA_REQMASKCLR_R |= 1 << channelNumber;                        // Allow DMA controller to see burst requests from the channel
}


void debugDma(char* buffer)
{
    putsUart0("---------DMA DEBUG BEGIN---------\n");

    sprintf(buffer, "uDMA controller configured to use %d channels\n", ((UDMA_STAT_R & UDMA_STAT_DMACHANS_M) >> UDMA_STAT_DMACHANS_S) + 1);
    putsUart0(buffer);

    putsUart0("uDMA controller state: ");
    switch ((UDMA_STAT_R & UDMA_STAT_STATE_M) >> 4)
    {
        case 0: putsUart0("IDLE"); break;
        case 1: putsUart0("READING CHANNEL CCONTROLLER DATA"); break;
        case 2: putsUart0("READING SOURCE END POINTER"); break;
        case 3: putsUart0("READING DESTINATION END POINTER"); break;
        case 4: putsUart0("READING SOURCE DATA"); break;
        case 5: putsUart0("WRITING DESTINATION DATA"); break;
        case 6: putsUart0("WAITING FOR UDMA REQUEST TO CLEAR"); break;
        case 7: putsUart0("WRITING CHANNEL CONTROLLER DATA"); break;
        case 8: putsUart0("STALLED"); break;
        case 9: putsUart0("DONE"); break;
        default: putsUart0("UNDEFINED"); break;
    }
    putcUart0('\n');

    putsUart0("uDMA controller ");
    putsUart0((UDMA_STAT_R & UDMA_STAT_MASTEN) ? "enabled\n" : "disabled\n");

    sprintf(buffer, "DMA channel control base pointer: 0x%x\n", UDMA_CTLBASE_R);
    putsUart0(buffer);

    sprintf(buffer, "DMA alternate channel control base pointer: 0x%x\n", UDMA_ALTBASE_R);
    putsUart0(buffer);

    putsUart0("Channels waiting on a request: ");
    int i;
    for (i = 0; i < sizeof(UDMA_WAITSTAT_R)*8; i++)
    {
        if(UDMA_WAITSTAT_R & (1 << i))
        {
            sprintf(buffer, "%d ", i);
            putsUart0(buffer);
        }
    }
    putcUart0('\n');

    putsUart0("Channels using primary control structure: ");
    for (i = 0; i < sizeof(UDMA_WAITSTAT_R)*8; i++)
    {
        if(!(UDMA_ALTSET_R & (1 << i)))
        {
            sprintf(buffer, "%d ", i);
            putsUart0(buffer);
        }
    }
    putcUart0('\n');

    putsUart0("Channels using alternate control structure: ");
    for (i = 0; i < sizeof(UDMA_WAITSTAT_R)*8; i++)
    {
        if(UDMA_ALTSET_R & (1 << i))
        {
            sprintf(buffer, "%d ", i);
            putsUart0(buffer);
        }
    }
    putcUart0('\n');

    putsUart0("---------DMA DEBUG END---------\n");
}
