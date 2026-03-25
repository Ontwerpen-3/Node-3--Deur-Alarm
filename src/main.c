
#define F_CPU 32000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include "clock.h"
#include "serialF0.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"

#define NRF_CHANNEL 101
#define MAXBUF 32
#define TOON1_PER 1000
#define TOON1_CCA 500
#define TOON2_PER 500
#define TOON2_CCA 250
#define TOONWISSELEN 200

#define PACKET_TYPE_EMERGENCY 0x03

static volatile uint8_t rx_flag = 0;

static volatile uint8_t AlarmAan = 0;
static volatile uint16_t teller = 0;
static volatile uint8_t toon = 0;
static volatile uint16_t alarmDuur = 0;

uint8_t rx_packet[MAXBUF];

uint8_t noodsieraad_pipe [5] = {'S','O','S','0','3'}; 

typedef struct 
{
    uint8_t packet_type;
    uint8_t emergency_active;
} 
emergency_packet_t;


void nrf_init(void)
{
 nrfspiInit(); 
 nrfBegin(); 
 nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_8RETRANSMIT_gc); 
 nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc); 
 nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc); 
 nrfSetCRCLength(NRF_CONFIG_CRC_16_gc); 
 nrfSetChannel(NRF_CHANNEL); 
 nrfSetAutoAck(0); 
 nrfEnableDynamicPayloads(); 
 nrfClearInterruptBits(); 
 nrfFlushRx();  
 nrfFlushTx(); 
 NRF24_IRQ_PORT.INT0MASK |= NRF24_IRQ_PIN; 
 NRF24_IRQ_PORT.NRF24_IRQ_CTRL = PORT_ISC_FALLING_gc; 

 NRF24_IRQ_PORT.INTCTRL |= (NRF24_IRQ_PORT.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;

 nrfOpenReadingPipe(0, (uint8_t *) noodsieraad_pipe); 

 nrfPowerUp(); 
 nrfStartListening(); 
}

ISR(NRF24_IRQ_VEC)
{
  rx_flag = 1;
}

ISR(TCE0_OVF_vect)
{
    if (AlarmAan) alarmDuur++;
    if (AlarmAan == 0) return;

    teller++;

    if (teller >= TOONWISSELEN)   
    {
        if (toon == 0)
        {
            TCE0.PER = TOON1_PER;  
            TCE0.CCA = TOON1_CCA;
            toon = 1;
        }
        else
        {
            TCE0.PER = TOON2_PER;    
            TCE0.CCA = TOON2_CCA;
            toon = 0;
        }
        teller = 0;
       
    }
}


int main(void)
{
    
    PORTD.DIRSET = PIN0_bm;
    PORTD.OUTCLR = PIN0_bm;
    PMIC.CTRL |= PMIC_LOLVLEN_bm;

    PORTE.DIRSET = PIN0_bm;   
    TCE0.PER = TOON1_PER;
    TCE0.CCA = TOON1_CCA; 
    TCE0.CTRLB = TC_WGMODE_SS_gc | TC0_CCAEN_bm;
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;   
    TCE0.CTRLA = TC_CLKSEL_OFF_gc;      
    PMIC.CTRL |= PMIC_LOLVLEN_bm;

init_clock();
init_stream(F_CPU);

PMIC.CTRL |= PMIC_LOLVLEN_bm;

nrf_init();
sei();
printf("Ik ontvang nu signalen\r\n");
 while (1)
    {
        if (rx_flag)
        {
            rx_flag = 0;

            uint8_t AlarmPipe;
            if (nrfAvailable(&AlarmPipe))
            {
                uint8_t length = nrfGetDynamicPayloadSize(); 

                if (length > MAXBUF)
                {
                    length = MAXBUF;
                }

                nrfRead(rx_packet, length);

                printf("Pakket ontvangen, lengte = %u, pipe = %u\r\n", length, AlarmPipe);

                
                if (length == sizeof(emergency_packet_t))
                {
                    emergency_packet_t *pkt = (emergency_packet_t *)rx_packet;

                    if (pkt->packet_type == PACKET_TYPE_EMERGENCY)
                    {
                        if (pkt->emergency_active == 1U)
                        {
                            
                            AlarmAan = 1;
                            alarmDuur = 0; 
                            PORTD.OUTSET = PIN0_bm; 

                            teller = 0;
                            toon = 0;
                            TCE0.PER = TOON1_PER;
                            TCE0.CCA = TOON1_CCA;
                            TCE0.CTRLA = TC_CLKSEL_DIV8_gc; 

                            printf("Er is een noodgeval gedetecteerd!\r\n");
                        }
                        else
                        {
                            
                            AlarmAan = 0;

                            PORTD.OUTCLR = PIN0_bm; 
                            TCE0.CTRLA = TC_CLKSEL_OFF_gc; 

                            printf("Noodsignaal UIT\r\n");
                        }
                    }
                    else
                    {
                        printf("!!Verkeerd packet type ontvangen!!\r\n");
                    }
                }
                else
                {
                    printf("!!Onbekende packet lengte: %u!!\r\n", length);
                }

                nrfClearInterruptBits();
            }
        }
        if (AlarmAan && alarmDuur >= 60000U)
        {
            AlarmAan = 0;
            PORTD.OUTCLR = PIN0_bm;
            TCE0.CTRLA = TC_CLKSEL_OFF_gc;
            printf("Alarm gaat uit na 15 secondes\r\n");
        }
    }
}