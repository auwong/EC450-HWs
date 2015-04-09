/* 4-1-2015
 SPI_bounce_master
 At regular WDT intervals, this sends data out the UCB SPI interface.
 The data received is logged by the RX interrupt handler.
 This example can be used to loop back (ie, connecting MOSI to MISO)
 A global counter keeps track of the numbers of TX and RX operations.

 Timing and clock.
 MCLK and SMCLK = 8 Mhz
 UCB0BRx interface divisor is a parameterized below.
 WDT divides SMCL by 512 (==> fastest rate gives 1 TX every 64 microseconds)
 Parameter ACTION_INTERVAL controls actual frequency of WDT interrupts that TX
 16 bit Parameter BIT_RATE_DIVISOR controls the SPI bitrate clock
 */

#include "msp430g2553.h"

#define ADC_INPUT_BIT_MASK 0x10
#define ADC_INCH INCH_4

/* declarations of functions defined later */
void init_adc(void);
void init_spi(void);
void init_wdt(void);

// Global variables and parameters (all volatilel to maintain for debugger)
volatile int latest_result;   // most recent result is stored in latest_result
volatile unsigned long conversion_count=0; //total number of conversions done

volatile unsigned char data_to_send = 0;	// current byte to transmit
volatile unsigned long tx_count = 0;		// total number of transmissions
volatile unsigned char data_received= 0; 	// most recent byte received
volatile unsigned long rx_count=0;			// total number received handler calls

// Try for a fast send.  One transmission every 64 microseconds
// bitrate = 1 bit every 4 microseconds
#define ACTION_INTERVAL 1
#define BIT_RATE_DIVISOR 32

// ===== Watchdog Timer Interrupt Handler ====

volatile unsigned int action_counter=ACTION_INTERVAL;

/*
 * The ADC handler is invoked when a conversion is complete.
 * Its action here is to simply store the result in memory.
 */
void interrupt adc_handler(){
	latest_result=ADC10MEM;   // store the answer
	++conversion_count;       // increment the total conversion count
}
ISR_VECTOR(adc_handler, ".int05")

// Initialization of the ADC
void init_adc(){
	ADC10CTL1= ADC_INCH	//input channel 4
			+SHS_0 //use ADC10SC bit to trigger sampling
			+ADC10DIV_4 // ADC10 clock/5
			+ADC10SSEL_0 // Clock Source=ADC10OSC
			+CONSEQ_0; // single channel, single conversion
	;
	ADC10AE0=ADC_INPUT_BIT_MASK; // enable A4 analog input
	ADC10CTL0= SREF_0	//reference voltages are Vss and Vcc
			+ADC10SHT_3 //64 ADC10 Clocks for sample and hold time (slowest)
			+ADC10ON	//turn on ADC10
			+ENC		//enable (but not yet start) conversions
			+ADC10IE  //enable interrupts
			;
}

interrupt void WDT_interval_handler(){
	ADC10CTL0 |= ADC10SC; // trigger a conversion
	if (--action_counter==0){
		UCB0TXBUF=latest_result; // init sending current byte
		//++data_to_send; // increment byte to send for next time
		++tx_count;
		action_counter=ACTION_INTERVAL;
	}
}
ISR_VECTOR(WDT_interval_handler, ".int10")

void init_wdt(){
	// setup the watchdog timer as an interval timer
	// INTERRUPT NOT YET ENABLED!
	WDTCTL =(WDTPW +		// (bits 15-8) password
			// bit 7=0 => watchdog timer on
			// bit 6=0 => NMI on rising edge (not used here)
			// bit 5=0 => RST/NMI pin does a reset (not used here)
			WDTTMSEL +     // (bit 4) select interval timer mode
			WDTCNTCL  		// (bit 3) clear watchdog timer counter
			// bit 2=0 => SMCLK is the source
			// bits 1-0 = 10=> source/512
	);
	IE1 |= WDTIE; // enable WDT interrupt
}

//----------------------------------------------------------------

// ======== Receive interrupt Handler for UCB0 ==========

void interrupt spi_rx_handler(){
	data_received=UCB0RXBUF; // copy data to global variable
	++rx_count;				 // increment the counter
	IFG2 &= ~UCB0RXIFG;		 // clear UCB0 RX flag
}
ISR_VECTOR(spi_rx_handler, ".int07")


//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)

void init_spi(){
	UCB0CTL1 = UCSSEL_2+UCSWRST;  		// Reset state machine; SMCLK source;
	UCB0CTL0 = UCCKPH					// Data capture on rising edge
			// read data while clock high
			// lsb first, 8 bit mode,
			+UCMST					// master
			+UCMODE_0				// 3-pin SPI mode
			+UCSYNC;					// sync mode (needed for SPI or I2C)
	UCB0BR0=BRLO;						// set divisor for bit rate
	UCB0BR1=BRHI;
	UCB0CTL1 &= ~UCSWRST;				// enable UCB0 (must do this before setting
	//              interrupt enable and flags)
	IFG2 &= ~UCB0RXIFG;					// clear UCB0 RX flag
	IE2 |= UCB0RXIE;					// enable UCB0 RX interrupt
	// Connect I/O pins to UCB0 SPI
	P1SEL =SPI_CLK+SPI_SOMI+SPI_SIMO;
	P1SEL2=SPI_CLK+SPI_SOMI+SPI_SIMO;
}


/*
 * The main program just initializes everything and leaves the action to
 * the interrupt handlers!
 */

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
	DCOCTL  = CALDCO_8MHZ;

	init_spi();
	init_adc();
	init_wdt();
	_bis_SR_register(GIE+LPM0_bits);

}
