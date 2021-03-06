#include <msp430g2553.h>

volatile unsigned int blink_interval[]={67,67,67,67,67,201,201,67,201,67,201,201,67,67,67,67,67,469}; // number of WDT interrupts per blink of LED
volatile unsigned int blink_counter;   // down counter for interrupt handler
volatile unsigned int blink_index;

int main(void) {
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

	  P1DIR |= 0x01;					// Set P1.0 to output direction

	  // initialize the state variables
	  blink_index=0;					// initialize index
	   // the number of WDT interrupts per toggle of P1.0
	  blink_counter=blink_interval[blink_index];     // initialize the counter


	  _bis_SR_register(GIE+LPM0_bits);  // enable interrupts and also turn the CPU off!
}

// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of about 8K/1.1MHz ~= 7.4ms.

interrupt void WDT_interval_handler(){
  if (--blink_counter==0){          // decrement the counter and act only if it has reached 0
	  P1OUT ^= 1;                   // toggle LED on P1.0
	  if (blink_index>=17)
		  blink_index=0;
	  else
		  blink_index++;
	  blink_counter=blink_interval[blink_index]; // reset the down counter
  }
}
// DECLARE function WDT_interval_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(WDT_interval_handler, ".int10")
