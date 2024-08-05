/* 
 * File:   main.c
 * Author: papizh
 *
 * Created on July 30, 2024, 8:42 PM
 */

#include <xc.h>
#include <string.h>
#define _XTAL_FREQ 4000000


#pragma config FOSC=INTRCIO , WDTE=OFF , PWRTE=OFF , MCLRE=OFF , CP=OFF , BOREN=OFF , IESO=OFF , FCMEN=OFF

/*Timer0 is used to calculate time to run pulse for power supply
 number of timer0 ticks needed for specific time period for 50Hz grid*/
const unsigned char Timer0Mode50[2] = {256-109, 256-0};                         //Pulse shifting by 70%, 0%                   (100% = 156.25)
//number of timer0 ticks needed for specific time period for 60Hz grid
const unsigned char Timer0Mode60[2] = {256-78, 256-0};                          //Pulse shifting by 60%, 0%                   (100% = 130.21)
const unsigned char* Timer0Mode;                                                //Pointer to array which suitable for current power grid

/*Timer2 is used to calculate time to run pulse for motor
number of timer2 ticks needed for specific time period for 50Hz grid*/
const unsigned char Timer2Mode50[6] = {213, 195, 173, 153, 133, 113};           //Pulse shifting by 85%, 78%, 69%, 61%, 53%, 45%    (100% = 250)
//number of timer2 ticks needed for specific time period for 60Hz grid
const unsigned char Timer2Mode60[6] = {177, 162, 144, 127, 110, 94};            //Pulse shifting by 85%, 78%, 69%, 61%, 53%, 45%    (100% = 208.3)
const unsigned char* Timer2Mode;

unsigned char powerSupplyMode = 0;                                              //Used to choose time from Timer0Mode array
unsigned char motorMode = 0;                                                    //Used to choose time from Timer2Mode array
unsigned char motorShift = 0;                                                   //Pulse shift for motor soft start

void __interrupt() isr(void)                                                    //On interruption event
{
    if (RABIE && RABIF) {                                                       //On sine crossing 0 interruption
        PORTB;                                                                  //Make a read from PORTB to end mismatch
        RABIF = 0x00;                                                           //Reset interrupt flag
        TMR2 = 0x00;                                                            //Reset timer2
        TMR0 = Timer0Mode[powerSupplyMode];                                     //Set time on timer0 so it would make timed overflow
        PR2 = Timer2Mode[motorMode]+motorShift;                                 //Set overflow target for timer2
        if(motorShift>0) motorShift-=2;                                         //"Soft start 1% step" (0.8% 50Hz, 0.96% 60Hz)
        if (powerSupplyMode==1) T0IF=1;                                         //If power mode 1 then set flag to immediately run power pulse
    }
    if (T0IE && T0IF) {                                                         //On timer0 overflow
        T0IF = 0;                                                               //Reset interrupt flag
        RC7 = 0;                                                                //Start power supply pulse
        __delay_us(20);                                                         //Delay for 20 us
        RC7=1;                                                                  //Stop power supply pulse
    }
    if (TMR2IE && TMR2IF) {                                                     //On timer2 event
        TMR2IF = 0;                                                             //Reset interrupt flag
        RA1 = 0;                                                                //Start drive pulses
        RA2 = 0;
        __delay_us(20);                                                         //Delay for 20 us
        RA1 = 1;
        RA2 = 1;                                                                //Stop drive pulses
        PR2 = 0xff;                                                             //Set period register to max
    }
}

//Switches LEDs based on bits in passed number
void SetLEDPinBits(unsigned char LedBits) {                                     //Order is "- - - led5 led4 led3 led2 led1"
    RA4 = 0b1&LedBits;
    RC5 = 0b1&(LedBits>>1);
    RC4 = 0b1&(LedBits>>2);
    RB7 = 0b1&(LedBits>>3);
    RA5 = 0b1&(LedBits>>4);
}
void SetLEDPinInt(unsigned char Level) {                                        //Sets LEDs based on input motor level
    unsigned char LedBits = 0;
    for (unsigned char i = 0; i<Level; i++){
        LedBits <<= 1;
        LedBits |= 0b1;
    }
    SetLEDPinBits(LedBits);
}


/*
 * 
 */
void main(void) {
    /*Start of initialization*/
    IRCF2 = 1; IRCF1 = 1; IRCF0 = 0;    //Fosc = 4 MHz
    SCS = 1;                            //Choose internal oscillator
    //Disable analog input mode
    ANSEL = 0x00;
    ANSELH = 0x00;
    
    //Port initialization
    TRISA = 0b00001001;
    PORTA = 0b00110110;
    TRISB = 0b01100000;
    PORTB = 0b10010000;
    TRISC = 0b01001100;
    PORTC = 0b10110011;
    
    //set port RB5 to interrupt on change
    IOCB5 = 1;
    
    //Timer0 setup. Timer0 counts for power supply timing, by triggering overflow event.    
    T0CS = 0; PSA = 0; PS2 =1; PS1 = 1; PS0 = 1;                                //Prescaler is 1:64, 15.625 kHz
    //Timer2 setup. Timer2 counts for drive control timing by achieving PR2.
    T2CON = 0b01001110;                                                         //Postscaler 10,Timer2 Off , Prescaler = 4, 25.000 kHz
    
    //Disable interrupts
    INTCON = 0;                                                                 //Disable all interrupt enables and flags
    RABIE = 0;
    T0IE = 0;
    TMR2IE = 0;
    /*End of initialization*/
    
    //checking for power grid frequency
    while(!RB5) {} 
    while(RB5) {}                                                               //Wait until sine crosses 0 falling;
    TMR0 = 0x00;
    while(!RB5) {}                                                              //Wait until sine crosses 0 rising;
    {
        if (TMR0>143)                                                           //If timer were going for longer then 0.0092s then it is 50 Hz grid
        {
            Timer0Mode = Timer0Mode50;
            Timer2Mode = Timer2Mode50;
        } else                                                                  //For 60Hz grid
        {
            Timer0Mode = Timer0Mode60;
            Timer2Mode = Timer2Mode60;
        }
    }
    GIE = 1;                                                                    //Globally enable interruptions
    //Start sync with sine by enabling interrupts
    PORTB;                                                                      //Read PORTB to clear mismatch
    RABIF = 0;                                                                  //Reset interrupt trigger
    RABIE = 1;                                                                  //Enable PORTA and PORTB interrupt on change
    TMR0 = 0x00;                                                                //Reset Timer0
    T0IF = 0;                                                                   //Reset Timer0 interrupt flag
    T0IE = 1;                                                                   //Enable Timer0 interrupt, activating power supply controlled pulse
    TMR2ON = 1;                                                                 //Activate Timer2
    PEIE = 1;                                                                   //Enable peripherals interrupt
    powerSupplyMode = 1;                                                        //Set power supply mode to full-power
    
    //wave-blink with LED
    RB4 = 0;
    RA4 = 0;
    __delay_ms(100);
    RA4 = 1;
    RC5 = 0;
    __delay_ms(100);
    RC5 = 1;
    RC4 = 0;
    __delay_ms(100);
    RC4 = 1;
    RB7 = 0;
    __delay_ms(100);
    RB7 = 1;
    RA5 = 0;
    __delay_ms(100);
    RA5 = 1;
    RB4 = 1;                                                                    //set D3 & D4 on
    __delay_ms(100);
    powerSupplyMode = 0;                                                        //set PowerSuply to low-power mode
    
    //Holders of button states
    unsigned char PWR_state = 1;
    unsigned char SWP_state = 1;                                                //state of PLUS button
    unsigned char SWM_state = 1;                                                //state of MINUS button
    //Infinite loop
    while(1) {
        if (RC2^PWR_state) {                                                    //If POWER button state changes
            if(RC2) {                                                           //On POWER button release
                TMR2IE = 0;                                                     //Motor trigger off
                SetLEDPinBits(0b0000000);                                       //Turn off LEDs
                powerSupplyMode = 0;                                            //Low-power mode
            } else
            {                                                                   //On POWER button close
                powerSupplyMode = 1;                                            //Set power supply to full power mode
                if (!RA3){                                                      //if level 1 drive power on start
                    motorMode = 1;
                    SetLEDPinBits(0b00000001);
                } else if (!RA0) {                                              //if level 5 drive power on start
                    motorMode = 5;
                    SetLEDPinBits(0b00011111);
                } else {                                                        //default level 3 drive power on start
                    motorMode = 3;
                    SetLEDPinBits(0b00000111);                    
                }
                motorShift = Timer2Mode[0] - Timer2Mode[motorMode];             //Set motor soft start pulse shift
                motorShift += motorShift%2;                                     //Make sure motorShift is even number
                if (RC3) {                                                      //If FILTER is in open state
                    PR2 = 0xff;                                                  //Set period register at max
                    TMR2IF = 0;                                                 //Clear Timer2 flag
                    TMR2IE = 1;                                                 //Enabling timer 2 interruptions
                }
            }
            PWR_state = RC2;
        }
        if (!PWR_state) {                                                       //If powered on
            if (RA3^SWM_state){                                                 //If MINUS button changes state
                if (RA3&&(motorMode>1)) {                                       //If MINUS button were released
                    motorMode--;                                                //Decrement motor mode
                    SetLEDPinInt(motorMode);                                    //Set LEDs into appropriate order
                    if (motorShift>Timer2Mode[motorMode] - Timer2Mode[motorMode+1]){    //If still soft-starting
                        motorShift -= Timer2Mode[motorMode] - Timer2Mode[motorMode+1];  //Do motor pulse shift correction
                        motorShift += motorShift%2;                                     //Make motorShift even
                    }
                }
                SWM_state = RA3;                                                //Update SWM_state
            }
            if (RA0^SWP_state){                                                 //If PLUS button changes state
                if (RA0&&(motorMode<5)) {                                       //If PLUS button were released
                    motorMode++;                                                //Increment motor mode
                    motorShift += Timer2Mode[motorMode-1] - Timer2Mode[motorMode];//Setup motor shift
                    motorShift += motorShift%2;                                 //Make sure motorShift divisible by 2
                    SetLEDPinInt(motorMode);                                    //Set LEDs into appropriate order
                }
                SWP_state = RA0;                                                //Update SWP_state
            }
            if (!RC3){                                                          //On FILTER closed state
                TMR2IE = 0;                                                     //Disable timer2 interruptions, no more motor pulses
                while(!RC3){                                                    //Until FILTER open blink LEDs with motor level
                    SetLEDPinInt(0b00000000);
                    __delay_ms(200);
                    SetLEDPinInt(motorMode);
                    __delay_ms(200);
                }
                motorShift = Timer2Mode[0] - Timer2Mode[motorMode];             //Set motor soft start pulse shift
                motorShift += motorShift%2;                                     //Make sure motorShift is even number
                SetLEDPinInt(motorMode);                                        //Turn on LEDs with current motor Mode
                PR2 = 0xff;                                                     //Set Timer2 period register at max
                TMR2IF = 0;                                                     //Clear Timer2 flag
                TMR2IE = 1;                                                     //Enable timer2 interruptions, enables motor pulses
            }
        }
        
    }
}

