// run at 32 MHz
// 400 - 1000 Hz
// 

//x_siren.c
//one timeer creates the tone in freq generation mode
//one timer creates an interrupt to update the setting for frequency
//another timer creates the pwm signal for the volume control

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>

#define TRUE  1
#define FALSE 0


/***********************************************************************/
//               32Mhz internal RC oscillator initalization
void init_32Mhzclock(void){
//setup clock for 32Mhz RC internal oscillator
  OSC.CTRL = OSC_RC32MEN_bm; // enable 32MHz clock
  while (!(OSC.STATUS & OSC_RC32MRDY_bm)); // wait for clock to be ready
  CCP = CCP_IOREG_gc; // enable protected register change
  CLK.CTRL = CLK_SCLKSEL_RC32M_gc; // switch to 32MHz clock
}
/***********************************************************************/

/***********************************************************************/
//                 Initalize TCC0 to normal mode
//TCCO is used to as an interrupt source for updating the frequency of
//the tone being created. Thus, it does not need to use an output pin.
//Running in normal mode does not do any port override if no WGM mode
//is set and CC channel is not enabled. See pg. 173 of AU manual.
void init_TCC0_normal(){ //changes variable to change frequency / move tone
//setup timer for normal mode, interrupt on overflow
 
  TCC0_CTRLA     |= TC_CLKSEL_DIV1024_gc; //~440uS
  TCC0_CTRLB     |= TC_WGMODE_NORMAL_gc;
  TCC0_PER       = 0x00F0;
  TCC0_INTCTRLA  = 0x01;
  
}
/***********************************************************************/

/***********************************************************************/
//            Initalize TCD0 to frequency generation mode
//TCDO is used to create the tone at an output pin. Period time is set by 
//the CCA register instead of PER. No interrupts are used. Output is
//on PORTD bit 5. CCAEN must be enabled toS allow waveform generation.
//Frequency is defined by: clk_perif/[(2*prescaler_value)(CCA+1)]
// clk perif = our 32 Mhz clock signal
// prescaler_value is TC_CLKSEL_DIV64gc
void init_TCD0_freq_gen(){ // sets frequency
  TCD0_CTRLA |= TC_CLKSEL_DIV64_gc; //64
  TCD0_CTRLB |= TC_WGMODE_FRQ_gc | TC0_CCAEN_bm;
  TCD0_CCABUF = 0x237;//currentFrequency; // wass 237 , interrupt level bit zero set , shooting for 440Hz, so CCA should be 567
}
/***********************************************************************/

/***********************************************************************/
//             Initalize TCD1 to pwm, single slope mode
//TCD1 is used to create the PWM signal for adjusting the volume of the
//audio amplifier. No interrupts are used. Output is on pin PD4.
//CCAEN must be enabled to allow waveform generation. A 30% duty cycle
//is a good starting place for the volume level using 8 ohm speaker.
//pwm_freq = fclk/prescale*(PER+1)
//
void init_TCD1_pwm(){
 
  TCD1_CTRLA |= TC_CLKSEL_DIV8_gc; //sysclock div 8  
  TCD1_CTRLB |= TC_WGMODE_SINGLESLOPE_gc | TC1_CCAEN_bm; //single slope, pin on
  TCD1_PER   =  100; //Period of PWM signal. TOP is held here. 
  TCD1_CCA   =  30; //Duty cycle is set here. Smaller #'s, Duty cycle goes down.
  
}
/***********************************************************************/

/***********************************************************************/
//                       ISR for TCC0 
//Overflow will occur with TCD0_CCA match. Increases/decreases frequency
//as a fraction of the frequency, not as a fixed value. 
//

volatile uint16_t currentFrequency = 0x237; // Initial frequency 400Hz
volatile uint16_t targetFrequency = 0x237;  // Initial target frequency 400Hz
volatile uint16_t stepSize = 5; 

ISR(TCC0_OVF_vect){

// this is what itterates through the different frequencies
// 0x0237 corresponds to 400 Hz
// 0x3EB corresponds to 1000 Hz

      if (currentFrequency < targetFrequency) {  // If the target frequency is LARGER than the current frequency (counting up)
        currentFrequency += stepSize; // count UP by increasing the current frequency by the step size (5)
        if (currentFrequency > targetFrequency) { // if after increasing, we go over the target frequency
            currentFrequency = targetFrequency; // normalize the frequency to be the target (so it doesn't load something over 1000 Hz)
        }
    } else if (currentFrequency > targetFrequency) { // If the target frequency is SMALLER than the current frequency (counting down)
        currentFrequency -= stepSize; // count DOWN by decreasing the current frequency by the step size (5)
        if (currentFrequency < targetFrequency) { // if after decreasing, we go below the target frequency
            currentFrequency = targetFrequency; // normalize the frequency to be the target (so it doesn't load something under 400 Hz)
        }
    }

    TCD0_CCABUF = currentFrequency; // Set the new frequency

    if (currentFrequency == 0x237) {  //Change direction when reached 400 Hz
        targetFrequency = 0x3EB; // by setting new target frequemcy to 1000 Hz
    } else if (currentFrequency == 0x3EB) { // Change direction when reached 1000Hz
        targetFrequency = 0x237; // by setting new target frequemcy to 400 Hz
    }
    
  //somewhere in your code....
 // TCD0_CCABUF =  //Must use double buffering else, big pauses will
                  //occur as the TCD0 has to go to old value. Tricky!
}
/***********************************************************************/

int main(){

  init_32Mhzclock();
  init_TCD0_freq_gen(); //turn on frequency generation of tone
  init_TCC0_normal();   //turn on interrupt source for update of frequency
  init_TCD1_pwm();      //turn on pwm to control volume control

  PORTD.DIR |= PIN0_bm; //Port D bit 0 is the output for the tone
  PORTD.DIR |= PIN4_bm; //Port D bit 4 is the pwm for the volume control

  PMIC.CTRL |= 0x01;    //low level interrupts enabled
  sei();                //global interrupts on
  while(1){ }           //loop forever
}  // main

