// Took some code from the tone4 example and added the other necessary things
// Plays Joy to the World and the Chocobo theme song from the game series Final Fantasy
// Toggled on and off with a button
//-----------------------
#include "msp430g2553.h"
//-----------------------
// define the bit mask (within P1) corresponding to output TA0
#define TA0_BIT 0x02

// define the port and location for the button (this is the built in button)
// specific bit for the button

#define RED 0x01			//red LED
#define GREEN 0x40			//green LED
#define SP_BUTTON 0x10		// Start/Pause
#define R_BUTTON 0x80		// Reset
#define UP_BUTTON 0x04		// Increase speed
#define DOWN_BUTTON 0x20	// Decrease speed
//----------------------------------

// Some global variables

	//Index			//0			2		  4		5		  7			9		 11	  12  13 14   15 	 17
	//Notes			//C4		D4		  E4	F4		  G4  		A4		 B4	  C5  D5 E5   F5 	 G5
const int tones[] = {1900,1793,1693,1598,1508,1423,1344,1268,1196,1129,1066,1006,950,846,754,710,672,634};

//Duration of each note in Joy to the World
const int joyTimes[] = {44,33,11, 66,22, 44,44, 66,22,
						66,22, 66,22, 66,22, 22,22,22,22, 33,11,22,22,
						22,22,22,22, 33,11,22,22, 22,22,22,11,11, 66,11,11, 22,22,22,11,11,
						66,11,11, 22,44,22, 33,11,22,22, 44,44, 88};
//The index of tone that will be played for Joy to the World
const int joyIndex[] = {12,11,9, 7,5, 4,2, 0,7,
						9,9, 11,11, 12,12, 12,11,9,7, 7,5,4,12,
						12,11,9,7, 7,5,4,4, 4,4,4,4,5, 7,5,4, 2,2,2,2,4,
						5,4,2, 0,12,9, 7,5,4,5, 4,2, 0};

//Duration of each note in Chocobo Theme song
const int chocoboTimes[] = {22,11,11,11,11,11,11, 22,22,33,11, 11,5,6,11,11,33,11,
		11,5,6,11,11,44, 22,11,11,11,11,11,11, 22,22,33,11,
		11,5,6,11,11,33,11,	11,5,6,11,11,44, 22,11,11,11,11,11,11,
		22,22,33,11, 22,11,11,11,11,11,11, 11,5,6,11,11,44, 22,11,11,11,11,11,11,
		22,22,33,11, 11,5,6,11,11,33,11, 11,5,6,11,11,22,22, 88};
//The index of tone that will be played for Chocobo Theme song
const int chocoboIndex[] = {13,11,7,4,13,11,7, 11,7,11,9, 7,7,9,7,5,7,5,
		7,7,11,13,14,15, 13,11,7,4,13,11,7, 11,7,11,9,
		7,7,9,7,5,7,5, 7,7,11,13,14,15, 14,12,9,6,9,12,14,
		13,17,13,11, 12,9,6,2,6,9,12, 11,11,12,11,9,11, 14,12,9,6,9,12,14,
		13,17,13,11, 9,9,11,9,7,9,7, 9,9,11,12,13,14,16, 17};

int indexTones = 0;		//index for which tone to choose out of the tones array
int indexDur = 0;		//index for choosing which duration out of the songs' arrays
unsigned int tempo = 0;		//this isn't the tempo, it's really just the count until next note
unsigned int scoreNum = 1;		//Choose which score to play: 1 for Joy, 2 for Chocobo

volatile unsigned char SP_last_button;		//Saves last state for each button
volatile unsigned char R_last_button;
volatile unsigned char UP_last_button;
volatile unsigned char DOWN_last_button;

volatile float scale = 1;
volatile unsigned halfPeriod; // half period count for the timer
volatile unsigned long intcount=0; // number of times the interrupt has occurred
volatile unsigned soundOn=0; // state of sound: 0 or OUTMOD_4 (0x0080)
volatile unsigned pauseOn=0;	//for pauses between notes

void init_timer(void); // routine to setup the timer
void init_button(void); // routine to setup the buttons

// ++++++++++++++++++++++++++
void main(){
	//WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer
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

	BCSCTL1 = CALBC1_1MHZ; // 1Mhz calibration for clock
	DCOCTL = CALDCO_1MHZ;

	halfPeriod = tones[joyIndex[indexTones]];
	//halfPeriod=maxHP; // initial half-period at lowest frequency
	init_timer(); // initialize timer
	init_button(); // initialize the button
	_bis_SR_register(GIE+LPM0_bits);// enable general interrupts and power down CPU
}

// +++++++++++++++++++++++++++
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

// +++++++++++++++++++++++++++
void interrupt sound_handler(){
	TA0CCR0 = halfPeriod-1;
	// change half period if the sound is playing
	if (soundOn && tempo>=(scoreNum == 1 ? scale*2*joyTimes[indexDur] : scale*2*chocoboTimes[indexDur])){
		indexTones++;		//increment tones index
		halfPeriod = scoreNum == 1 ? tones[joyIndex[indexTones]] : tones[chocoboIndex[indexTones]];
		pauseOn = 1;
		tempo=0;
		indexDur++;			//increment duration index
		soundOn ^= OUTMOD_4;	//Switch sound off for the pause between notes
		if (scoreNum==1){		//If it's playing Joy to the World
			if (indexTones==sizeof(joyIndex)/2-1)		//Reset index
				indexTones=-1;
			if (indexDur==sizeof(joyTimes)/2){			//Reset duration
				indexDur=0;
				pauseOn = 0;	//Song ended, it's not a pause
				P1OUT ^= RED;
			}
		} else {				//If it's playing Chocobo Theme
			if (indexTones==sizeof(chocoboIndex)/2-1)	//Reset index
				indexTones=-1;
			if (indexDur==sizeof(chocoboTimes)/2){		//Reset duration
				indexDur=0;
				pauseOn = 0;	//Song ended, it's not a pause
				P1OUT ^= RED;
			}
		}
	} else if (pauseOn!=0 && tempo>=5){
		tempo = 0;
		pauseOn = 0;
		soundOn ^= OUTMOD_4;
	}
	TA0CCTL0 = CCIE + soundOn; //  update control register with current soundOn
	++intcount; // advance debug counter
}
ISR_VECTOR(sound_handler,".int09") // declare interrupt vector

// +++++++++++++++++++++++++++
// Button input System
// Button toggles the state of the sound (on or off)
// action will be interrupt driven on a downgoing signal on the pin

void init_button(){
// All GPIO's are already inputs if we are coming in after a reset
	P1DIR |= RED+GREEN;		//output LEDs
	P1OUT ^= RED;		//Turn off LEDs
	P1OUT |= SP_BUTTON+R_BUTTON+UP_BUTTON+DOWN_BUTTON; // pullup
	P1REN |= SP_BUTTON+R_BUTTON+UP_BUTTON+DOWN_BUTTON; // enable resistor
}

void interrupt WDT_interval_handler(){
  	unsigned char SP_b;			// temp buttons for current button state
  	unsigned char R_b;
  	unsigned char UP_b;
  	unsigned char DOWN_b;
  	SP_b= (P1IN & SP_BUTTON);  // read the BUTTON bit
  	R_b = (P1IN & R_BUTTON);
  	UP_b= (P1IN & UP_BUTTON);  // read the BUTTON bit
  	DOWN_b= (P1IN & DOWN_BUTTON);  // read the BUTTON bit

  	if (SP_last_button && (SP_b==0)){ // has the button bit gone from high to low
  		P1OUT ^= RED; // toggle both LED's
  		soundOn ^= OUTMOD_4; // flip bit for outmod of sound
  	} else if(R_last_button && (R_b==0)){
  		if (soundOn ^ pauseOn/*indexTones!=0 && indexDur!=0*/){		//Reset everything
			P1OUT &= ~RED;
			P1OUT |= GREEN;
			tempo = 0;
			scale = 1;
			soundOn &= ~OUTMOD_4;
			pauseOn = 0;
			indexTones = 0;
			indexDur = 0;
			scoreNum = 1;
			halfPeriod = tones[joyIndex[indexTones]];
  		} else if (scoreNum==1&&tempo==0){		//Change to Chocobo if current song is Joy
  			scoreNum = 2;
  			halfPeriod = tones[chocoboIndex[indexTones]];
  			P1OUT ^= GREEN;
  		} else if (scoreNum==2&&tempo==0){		//Change to Joy if current song is Chocobo
  			scoreNum = 1;
  			halfPeriod = tones[joyIndex[indexTones]];
  			P1OUT ^= GREEN;
  		}
  	} else if (UP_last_button && (UP_b==0)){ // has the button bit gone from high to low
  		scale-=0.1;
  		if (scale<=0.1)
  			scale = 0.1;
  	} else if (DOWN_last_button && (DOWN_b==0)){ // has the button bit gone from high to low
  		scale+=0.2;
  		if (scale>5)
  			scale = 5;
  	}
  	if (soundOn || pauseOn)
  		tempo++;
  	SP_last_button=SP_b;    // remember button reading for next time.
  	R_last_button=R_b;    // remember button reading for next time.
  	UP_last_button=UP_b;    // remember button reading for next time.
  	DOWN_last_button=DOWN_b;    // remember button reading for next time.
}
ISR_VECTOR(WDT_interval_handler, ".int10")
