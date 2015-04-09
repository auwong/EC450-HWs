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

#define TA0_BIT 0x02
 /* declarations of functions defined later */
 void init_spi(void);
 void init_wdt(void);

// Global variables and parameters (all volatilel to maintain for debugger)

volatile unsigned char data_to_send = 0;	// current byte to transmit
volatile unsigned long tx_count = 0;		// total number of transmissions
volatile unsigned char data_received= 0; 	// most recent byte received
volatile unsigned long rx_count=0;			// total number received handler calls

volatile unsigned halfPeriod; // half period count for the timer
volatile unsigned soundOn=OUTMOD_4; // state of sound: 0 or OUTMOD_4 (0x0080)

// Try for a fast send.  One transmission every 64 microseconds
// bitrate = 1 bit every 4 microseconds
#define ACTION_INTERVAL 1
#define BIT_RATE_DIVISOR 32

// ===== Watchdog Timer Interrupt Handler ====

volatile unsigned int action_counter=ACTION_INTERVAL;

interrupt void WDT_interval_handler(){
	if (--action_counter==0){
		UCB0TXBUF=0x50; // init sending current byte
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
			   //+UCMST					// master
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

// +++++++++++++++++++++++++++
void interrupt sound_handler(){
	TA0CCR0 = halfPeriod-1;
	if (soundOn){ // change half period if the sound is playing
		//halfPeriod = UCB0RXBUF; 			// adjust the period
		if (/*data_received>=0xA0&&*/data_received<0xB0){
			halfPeriod = 2258;
		} else if (data_received>=0xB0&&data_received<0xC0){
			halfPeriod = 2012;
		} else if (data_received>=0xC0&&data_received<0xD0){
			halfPeriod = 1900;
		}
	}
	TA0CCTL0 = CCIE + soundOn; //  update control register with current soundOn
//	++intcount; // advance debug counter
}
ISR_VECTOR(sound_handler,".int09") // declare interrupt vector

// Sound Production System
void init_timer(){ // initialization and start of timer
	TA0CTL |=TACLR; // reset clock
	TA0CTL =TASSEL1+ID_0+MC_1; // clock source = SMCLK, clock divider=1, continuous mode,
	TA0CCTL0=soundOn+CCIE; // compare mode, outmod=sound, interrupt CCR1 on
	//TA0CCR0 = TAR+halfPeriod; // time for first alarm
	TA0CCR0 = halfPeriod-1;
	P1SEL|=TA0_BIT; // connect timer output to pin
	P1DIR|=TA0_BIT;
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
  	init_wdt();
  	init_timer(); // initialize timer
 	_bis_SR_register(GIE+LPM0_bits);



}
