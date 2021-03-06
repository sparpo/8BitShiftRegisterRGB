/*
* 8BitShiftRegisterRGB.c
*
* Created: 13/05/2020 00:48:03
* Author : sparpo
*/

/*
_/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/
_/        _/    _/  _/    _/  _/    _/  _/    _/  _/    _/
_/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/    _/
      _/  _/        _/    _/  _/  _/    _/        _/    _/
_/_/_/_/  _/        _/    _/  _/    _/  _/        _/_/_/_/
*/

/* 74HC595 pinout
_______   _______
|d1	  |___|  +5V| 
|d2			  d0|
|d3		  Serial|
|d4		   EnOut|
|d5		   Latch|
|d6			 Clk|
|d7			 Clr|
|GND		 d7'|
|_______________|
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#define F_CPU 16000000UL
#include <util/delay.h> //needed for blocking delays

//pins 2(latch), 4(clock), 7(data). Same as pins on arduino board
#define latchPin 2
#define clockPin 4
#define dataPin 7
#define clearPin 5

#define blockDelay 5

//functions
void init_ports(void);
void init_USART(void);
void init_timer0(void);
void sendmsg (char *s);
void point(uint8_t x,uint8_t y, int colour); // 1 bit colour. 1 bit for each red, green and blue, so there are 8 possible colour combinations
void rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int colour);
void clearData(void);
void drawData(uint8_t data[][8]);
void shiftOut(uint8_t myDataOut);


unsigned char qcntr = 0,sndcntr = 0;   /*indexes into the queue*/
unsigned char queue[100];       /*character queue*/

volatile int refreshFlag;// Flag for refreshing display



// uint8_t types are used a lot to make sure that data that will be outputted is 8 bits long


uint8_t data[3][8] = { 
	{	0b00000000, //RED
		0b01111110,
		0b01000110,
		0b01001010,
		0b01010010,
		0b01100010,
		0b01111110,
		0b00000000},
	
	{	0b11111111, //GREEN
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000},
	
	{	0b00000000, //BLUE
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000,
		0b00000000,
		0b11111111}
};

char msg[6] = "hello";

int main(void) {
	//init_USART();// Serial for debugging purposes
	init_ports();
	init_timer0();
	//clearData();
	sei();
	refreshFlag = 0;

	
	while (1) {

		/********************************************************************************/
		/* Display update loop. Every 16ms												*/
		/********************************************************************************/
		if(refreshFlag) {
			drawData(data);
			refreshFlag = 0;
		}
		
	}
	return 0;
}


void clearData() {
	for(int i=0; i<3; i++) {
		for(int j=0; j<8; j++) {
			data[i][j] = 0;
		}
	}
}


void rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int colour) { //draws rectangle
	for(int i = 0; i<w; i++) {
		for(int j =0; j<h; j++) {
			point(x+i,y-j, colour);
		}
	}
}
void point(uint8_t x, uint8_t y, int colour) { //actually puts data in array
	for(int i=0; i<3; i++) {
		if((1<<i)&colour) //For example, if colour is 2 (binary 010), the second bit is high so it will set colour to green. 4 (100) will set it to red. 5 (101) will turn red and blue on
		data[i][y] = data[i][y] | (1<<x); //sets to 1
		else
		data[i][y] = data[i][y] & ~(1<<x); //sets to 0
	}
	
}
void drawData(uint8_t myData[][8]) { //takes data and outputs it to 8x8 matrix
	uint8_t j = 0;
	PORTB = (1<<clearPin);
	PORTB = 0;
	PORTB = (1<<clearPin);
	for (j = 0; j <= 7; j++) {//560*20 = 11200us = 11.2ms
		
		//ground latchPin and hold low for as long as you are transmitting
		PORTD = PORTD & ~(1<<latchPin); //latch pin low
		_delay_us(blockDelay);
		//shift data into regs
		
		shiftOut(255);
		shiftOut(0);
		shiftOut(255);
		
		
		shiftOut(255);
		/*shiftOut((1<<j)); //grounds row. For example 10111111 means 6th row is on
		
		shiftOut(~myData[1][j]);  //outputs green data
		
		shiftOut(~myData[2][j]);  //outputs blue data
		
		shiftOut(~myData[0][j]);  //outputs red data*/
		


		//return the latch pin high to signal chip that it
		//no longer needs to listen for information
		
		PORTD = PORTD | (1<<latchPin); //latch pin high
		_delay_us(blockDelay);
	}
	
}
void shiftOut(uint8_t myDataOut) { //shifts 8 bits out to register
	
	// This shifts 8 bits out LSB first,
	//on the rising edge of the clock,
	//clock idles low

	//internal function setup
	uint8_t i=0;
	int pinState;
	
	//clear everything out just in case to
	//prepare shift register for bit shifting
	
	PORTD = PORTD & ~((1<<dataPin) | (1<<clockPin)); //clock & data pin low
	_delay_us(blockDelay);
	//for each bit in the byte myDataOut?
	for (i=0; i<=7; i++)  {
		
		PORTD = PORTD & ~(1<<clockPin); //clock low
		_delay_us(blockDelay);
		//if the value passed to myDataOut and a bitmask result
		// true then... so if we are at i=6 and our value is
		// %11010100 it would the code compares it to %01000000
		// and proceeds to set pinState to 1.
		if ( myDataOut & (1<<i) ) {
			pinState= 1;
		}
		else {
			pinState= 0;
		}
		
		
		//Sets the pin to HIGH or LOW depending on pinState
		PORTD = PORTD | (pinState<<dataPin);
		_delay_us(blockDelay);
		//register shifts bits on upstroke of clock pin
		PORTD = PORTD | (1<<clockPin);
		_delay_us(blockDelay);
		//zero the data pin after shift to prevent bleed through
		PORTD = PORTD & ~(1<<dataPin);
		_delay_us(blockDelay);
	}
	
	//stop shifting
	PORTD = PORTD & ~(1<<clockPin);
	_delay_us(blockDelay);
}

void sendmsg (char *s)
{
	qcntr = 0;    /*preset indices*/
	sndcntr = 1;  /*set to one because first character already sent*/
	
	queue[qcntr++] = 0x0d;   /*put CRLF into the queue first*/
	queue[qcntr++] = 0x0a;
	while (*s)
	queue[qcntr++] = *s++;   /*put characters into queue*/
	
	UDR0 = queue[0];  /*send first character to start process*/
}

/********************************************************************************/
/* Interrupt Service Routines													*/
/********************************************************************************/

/*this interrupt occurs whenever the */
/*USART has completed sending a character*/

ISR(USART_TX_vect)
{
	/*send next character and increment index*/
	if (qcntr != sndcntr)
	UDR0 = queue[sndcntr++];
}

ISR(TIMER0_OVF_vect) { //overflow every 16ms
	refreshFlag = 1; // Call for updating screen
	TCNT0 = 6; // 256-250 = 6
}

void init_ports() {
	DDRB = (1<<clearPin);
	PORTB = (1<<clearPin); //set high
	DDRD = (1<<latchPin) | (1<<clockPin) | (1<<dataPin); //enable pins as output
	PORTD = 0; //set all pins low
}
void init_USART() {
	UCSR0B = (1<<TXEN0) | (1<<TXCIE0); //enable transmitter and tx interrupt
	UBRR0 = 103; //baud 9600
}
void init_timer0() {
	TCCR0A = 0;
	TIMSK0 = (1<<TOIE0);	// enable interrupt
	TCCR0B = (5<<CS00); // prescalar 1024. 16Mhz/1024 = 15625Hz = 0.064ms
	// For refresh rate of 62.5Hz => 16ms /0.064ms = 250
	TCNT0 = 6; //256-250 = 6. To get a count of 250, must start at 6.
}
