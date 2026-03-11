
#define F_CPU 32000000UL
#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint8_t toggle = 0;
volatile uint8_t count = 0;

volatile uint16_t teller = 0;
volatile uint8_t toon = 0;

ISR(TCE0_OVF_vect)
{
    teller++;

    if (teller >= 200)   // wacht even voordat toon verandert
    {
        if (toon == 0)
        {
            TCE0.PER = 1000;   // toon 1
            TCE0.CCA = 500;
            toon = 1;
        }
        else
        {
            TCE0.PER = 500;    // toon 2
            TCE0.CCA = 250;
            toon = 0;
        }

        teller = 0;
    }
}

ISR(TCC0_OVF_vect) 
{ 
    count++; if (count >= 4) // 4 × 0.5s = 2 seconden 

    { 
        PORTD.OUTTGL = PIN0_bm; // wissel AAN/UIT 
        count = 0; 
    } 
}

int main(void)
{
    // ===== Solenoid output (PD0) =====
    PORTD.DIRSET = PIN0_bm;
    PORTD.OUTCLR = PIN0_bm;


    // ===== Timer C0 instellen =====
    TCC0.PER = 976;                 
    TCC0.CTRLA = TC_CLKSEL_DIV1024_gc; 
    TCC0.INTCTRLA = TC_OVFINTLVL_LO_gc;

    PMIC.CTRL |= PMIC_LOLVLEN_bm;

    {
    PORTE.DIRSET = PIN0_bm;   // PE0 output

    TCE0.PER = 1000;
    TCE0.CCA = 500;

    TCE0.CTRLB = TC_WGMODE_SS_gc | TC0_CCAEN_bm;

    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;   // overflow interrupt

    TCE0.CTRLA = TC_CLKSEL_DIV8_gc;      // start timer

    PMIC.CTRL |= PMIC_LOLVLEN_bm;
    sei();

    while (1)
    {
        //Leeg want alles gebeurt in interrupts
    }
}

}
