/***************************************************************************************
 *  Simple Button Example (version 1)
 *
 * In operation either the RED or GREEN LED is on.  Pressing the Launchpad button toggles
 * between them.  A global counter keeps track of the total number of button presses.
 *
 *  This version uses the Watchdog Timer ("WDT") in interval mode.
 *  The default system clock is about 1.1MHz.  The WDT is configured to be driven by this
 *  clock and produce an interrupt every 8K ticks ==> interrupt interval = 8K/1.1Mhz ~ 7.5ms
 *  When the WDT interrupt occurs, the WDT interrupt handler is called.
 *  This handler checks the state of the button and if it has been *pressed* since the last
 *  interrupt it toggles the two LEDs.
 *
 * Initially the green LED is on and the red LED is off.
 *
 *  NOTE: Between interrupts the CPU is OFF!
 ***************************************************************************************/

#include <msp430g2553.h>

// Bit masks for port 1
#define RED 0x01
#define GREEN 0x40
#define BUTTON 0x08

// Global state variables
volatile unsigned char last_button; // the state of the button bit at the last interrupt
volatile int switchInterval;
volatile int switchCounter;
volatile int runCounter;		// Triggers whether to start counting until switching modes
volatile int recordIndex;
volatile int record[20] = {};
volatile int recordDuration = 0;

void main(void) {
	  // setup the watchdog timer as an interval timer
	  WDTCTL =(WDTPW + // (bits 15-8) password
	                   // bit 7=0 => watchdog timer on
	                   // bit 6=0 => NMI on rising edge (not used here)
	                   // bit 5=0 => RST/NMI pin does a reset (not used here)
	           WDTTMSEL + // (bit 4) select interval timer mode
	           WDTCNTCL +  // (bit 3) clear watchdog timer counter
	  		          0 // bit 2=0 => SMCLK is the source
	  		          +1 // bits 1-0 = 01 => source/8K
	  		   );
	  IE1 |= WDTIE;		// enable the WDT interrupt (in the system interrupt register IE1)

	  //Variables
	  switchInterval = 300;
	  switchCounter = 0;
	  runCounter = 0;
	  recordIndex = 0;

	  P1DIR |= RED+GREEN;					// Set RED and GREEN to output direction
	  P1DIR &= ~BUTTON;						// make sure BUTTON bit is an input

	  P1OUT |= GREEN+RED; // Green on
	  //P1OUT &= ~RED;  // Red off
	  P1OUT &= ~GREEN;

	  P1OUT |= BUTTON;
	  P1REN |= BUTTON;						// Activate pullup resistors on Button Pin

	  _bis_SR_register(GIE+LPM0_bits);  // enable interrupts and also turn the CPU off!
}

// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of about 8K/1.1MHz ~= 7.4ms.

interrupt void WDT_interval_handler(){
  	unsigned char b;
  	volatile unsigned int i, j;
  	unsigned int zero = 0;
  	b= (P1IN & BUTTON);  // read the BUTTON bit
  	if (last_button && b==0){ // has the button bit gone from high to low
  		P1OUT &= ~RED;	//Guarantees to shut off red LED
  		if (runCounter == 1&&recordIndex<20){
  		  	record[recordIndex]=switchCounter;
  			recordIndex++;
  		}
  		if (recordIndex<20){			//Check that there's still recording space
  			P1OUT ^= GREEN; // toggle green LED on
  			while (b==0) {		//Start recording
  				recordDuration++;
  				b= (P1IN & BUTTON);
  				__delay_cycles(8000);
  			}
  			P1OUT ^= GREEN; // toggle green LED off
  			record[recordIndex]=recordDuration;
  			recordIndex++; //next save slot
  			recordDuration = 0;
  		} else {		//ELSE HANDLE CRASH GRACEFULLY
  			for (i = 0; i<4; i++){
				P1OUT ^= RED;
				__delay_cycles(8000);
			}
  		}
  		switchCounter = 0;
  		runCounter = 1;		//Starts countdown to the mode change
  	}
  	if (switchCounter == switchInterval){
  		P1OUT ^= RED;
  		//Playback
  		for (i = 0; i<recordIndex; i++){
  			P1OUT ^= GREEN;
  			j = record[i];
  			while (j>zero){
  				j--;
  				__delay_cycles(8000);
  			}
  		}
  		P1OUT &= ~GREEN;
  		recordIndex = 0;	//Reset index
  		switchCounter = 0;
  		runCounter = 0;		//Goes back to waiting
  		P1OUT ^= RED;
  	}
  	if (runCounter == 1)
  		switchCounter++;
  	last_button=b;    // remember button reading for next time.
}
// DECLARE function WDT_interval_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(WDT_interval_handler, ".int10")
