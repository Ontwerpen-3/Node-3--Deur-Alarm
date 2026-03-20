
#define F_CPU 32000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include "clock.h"
#include "serialF0.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"

#define NRF_CHANNEL 1000
#define MAXBUF 32
#define TOON1_PER 1000
#define TOON1_CCA 500
#define TOON2_PER 500
#define TOON2_CCA 250
#define TOONWISSELEN 200

volatile uint8_t rx_flag = 0;

volatile uint8_t AlarmAan = 0;

volatile uint16_t teller = 0;
volatile uint8_t toon = 0;

uint8_t rx_packet[MAXBUF];

uint8_t alarm_pipe [5] = "0pipe"; // De 0pipe luister alleen maar in principe en verstuurd geen data.
uint8_t noodsieraad_pipe [5] = "1pipe"; // de 1pipe die verstuurt data en ontvang je via de 0pipe.


void nrf_init(void)
{
 nrfspiInit(); // SPI Initialiseren
 nrfBegin(); // NRF op beginstand zetten
 nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_8RETRANSMIT_gc); // 1000 microSecondes tussen tussen retries, Probeert 8 keer te verzend als faalt.
 nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc); // Zendvermogen is -6db (Hoe dichter bij de 0 hoe beter het signaal)
 nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc); // Datasnelheid is 250 kbps, als je lager kiest dan heb je groter bereik maar langzamer.
 nrfSetCRCLength(NRF_CONFIG_CRC_16_gc); // Zet foutcontrole op een 16-bit CRC, betrouwbaarder dan 8-bit.
 nrfSetChannel(NRF_CHANNEL); // Kiest de NRF channel die je al hebt gedefined.
 nrfSetAutoAck(1); // Ontvanger stuurt automatisch een bericht dat de data goed is ontvangen
 nrfEnableDynamicPayloads(); // Data wordt variabel.
 nrfClearInterruptsBits(); // verwijderd alle interrupt flags
 nrfFlushRx(); //Leegt de ontvangen data 
 nrfFlushTx(); //Leegt de verzonden data
 NRF24_IRQ_PORT.INT0MASK |= NRF24_IRQ_PIN; // Mask is welke pin een interrupt mag geven
 NRF24_IRQ_PORT.NRF24_IRQ_CTRL = PORT_ISC_FALLING_gc; // Zorgt ervoor dat een pin van hoog (1) naar laag (0) gaat.

 NRF24_IRQ_PORT.INTCTRL |= (NRF24_IRQ_PORT.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;
 //INTCTRL is prioriteit stellen aan interrupt  
 //INT0LVL stelt het de prioriteit van de interrupt (dit geval is de interrupt een LOW priority)

 nrfOpenWritingPipe((uint8_t *) alarm_pipe); // Opent de pipe om data te versturen
 nrfOpenReadingPipe(0, (uint8_t *) noodsieraad_pipe); // Opent de pipe om data te ontvangen
 nrfPowerUp(); // Zet de NRF van standby op actief 
 nrfStartListening(); // begint met het luisteren naar de andere NRF om Data te ontvangen
}

ISR(PORTB_INT0_vect)
{
  rx_flag = 1;
}

ISR(TCE0_OVF_vect)
{

    if(alarmAan == 0) return;

    teller++;

    if (teller >= TOONWISSELEN)   // wacht even voordat toon verandert
    {
        if (toon == 0)
        {
            TCE0.PER = TOON1_PER;   // toon 1
            TCE0.CCA = TOON1_CCA;
            toon = 1;
        }
        else
        {
            TCE0.PER = TOON2_PER;    // toon 2
            TCE0.CCA = TOON2_CCA;
            toon = 0;
        }

        teller = 0;
    }
}


int main(void)
{
    // ===== Solenoid output (PD0) =====
    PORTD.DIRSET = PIN0_bm;
    PORTD.OUTCLR = PIN0_bm;
    PMIC.CTRL |= PMIC_LOLVLEN_bm;

    PORTE.DIRSET = PIN0_bm;   // PE0 output
    TCE0.PER = TOON1_PER;
    TCE0.CCA = TOON1_CCA; 
    TCE0.CTRLB = TC_WGMODE_SS_gc | TC0_CCAEN_bm;
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;   // overflow interrupt
    TCE0.CTRLA = TC_CLKSEL_OFF_gc;      // start timer
    PMIC.CTRL |= PMIC_LOLVLEN_bm;

    nrf_init();
    sei();

    while (1)
    {
        if (rx_flag)
        {
            rx_flag = 0;

            uint8_t AlarmPipe;
            if (nrfAvailable(&AlarmPipe))
            { 
                uint8_t length = nrfGetDynamicPayloadSize(); // vraagt op hoeveel bytes ontvangen zijn
                nrfRead(rx_packet, length); // leest de opgevraagde bytes uit

                if(rx_packet[0] == 'TIJDELIJK' || rx_packet[0] == 'TIJDELIJK2') // MOET 1 LETTER ZIJN UITEINDELIJK
                // Als 1 van de signalen binnenkomt gaat het alarm aan.
                {
                    alarmAan = 1;
                    PORTD.OUTSET = PIN0_bm;
                    TCE0.CTRLA = TC_CLKSEL_DIV8_gc;
                    
                    printf("Er is een noodgeval gedetecteerd!%c\n", rc_packet[0]);

                }
                    else
                    {
                        printf("!!Ontvangen signaal is niet correct!!%c\n", rx_packet[0]);
                    }
            }
            nrfClearInterruptsBits();
        }   
    }
  
}
